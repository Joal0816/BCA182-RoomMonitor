#include "dht22.h"
#include "FreeRTOS.h"
#include "task.h"

/* Only one port line can be addressed by this driver; a bounded loop keeps the
   index computation from running past the register width if a mis-sized pin
   mask (e.g. 0x0000) is ever passed in. */
#define DHT22_PIN_COUNT 16U

/* Longest level the sensor holds during a frame is 80 us.  Polling a GPIO with
   a direct register read costs only a few cycles per iteration, so this guard
   simply has to outlast a reply; it is a bound, not a calibrated delay.  Used
   only when the DWT cycle counter is unavailable. */
#define DHT22_EDGE_TIMEOUT 2000U

/* Host start signal.  The datasheet asks for the line to be held low for at
   least 1 ms; the sensor answers 20-40 us after the line is released. */
#define DHT22_START_LOW_MS 2U

/* Timeout bound for any single level of the reply: 80 us for each of the two
   response pulses, and 50 us low plus 26/70 us high per bit.  This is a bound,
   not an expected value, so it is deliberately generous -- it must still
   outlast a reply when the firmware's idea of the CPU clock (SystemCoreClock)
   and the rate DWT->CYCCNT actually advances at disagree, which is exactly the
   case on simulators that model the core clock and the RCC separately. */
#define DHT22_EDGE_US 1000U

/* Width of the sensor's response high pulse.  It is a fixed part of the
   protocol, so the driver times it and divides by this to learn how many
   counter cycles a microsecond really is. */
#define DHT22_RESPONSE_HIGH_US 80U

/* A data bit is a 26-28 us high pulse for "0" and 70 us for "1".  Measuring the
   high pulse and comparing it against the midpoint (48 us) does not depend on
   the CPU clock, whereas sampling at one fixed instant sits right on the
   boundary between the two encodings. */
#define DHT22_ONE_THRESHOLD_US 48U

/* STM32F1 GPIO configuration nibbles (CNF[1:0] mode[1:0]). */
#define DHT22_CNF_INPUT_PULL  0x8U /* input, pull-up/pull-down (ODR picks pull-up) */
#define DHT22_CNF_OUT_PP_2MHZ 0x2U /* output push-pull, 2 MHz */

#define DHT22_DWT_CYCCNTENA (1UL << 0UL)

static uint32_t DHT22_PinIndex(uint16_t pin) {
    uint32_t index = 0;
    while (index < DHT22_PIN_COUNT && (pin & (1UL << index)) == 0U) {
        index++;
    }
    return index;
}

/* Direction changes are a direct read-modify-write of the CRL/CRH nibble.
   HAL_GPIO_Init cannot be used here: it validates its arguments and touches RCC
   on every call, and the release-then-listen step has to complete within the
   few microseconds between the sensor letting go of the line and starting its
   reply. */
static void DHT22_SetConfig(DHT22_t *dht, uint32_t config) {
    uint32_t index = DHT22_PinIndex(dht->pin);
    volatile uint32_t *reg;
    uint32_t shift;
    uint32_t value;

    if (index < 8U) {
        reg = &dht->port->CRL;
        shift = index * 4U;
    } else {
        reg = &dht->port->CRH;
        shift = (index - 8U) * 4U;
    }

    value = *reg;
    value &= ~(0xFUL << shift);
    value |= config << shift;
    *reg = value;
}

static void DHT22_DriveLow(DHT22_t *dht) {
    dht->port->BRR = dht->pin;
    DHT22_SetConfig(dht, DHT22_CNF_OUT_PP_2MHZ);
}

/* Returning the line to an input is not enough on its own: with GPIO_NOPULL the
   pin is left floating as soon as the sensor stops driving it, so nothing pulls
   it back high and the reply never starts.  Selecting input with the internal
   pull-up (CNF = 10) and ODR = 1 is what supplies that bias. */
static void DHT22_Release(DHT22_t *dht) {
    dht->port->BSRR = dht->pin; /* ODR = 1 selects the pull-up, not the pull-down */
    DHT22_SetConfig(dht, DHT22_CNF_INPUT_PULL);
}

static inline uint8_t DHT22_IsHigh(const DHT22_t *dht) {
    return (dht->port->IDR & dht->pin) != 0U;
}

static uint32_t DHT22_UsToCycles(uint32_t us) {
    return us * (SystemCoreClock / 1000000U);
}

/* DWT_CTRL is a writable configuration register: it reads back set as soon as
   software writes it, even on cores where CYCCNT never actually advances.
   Treating that bit as proof the counter runs routes every timed loop into a
   full-length timeout spin, so probe the counter once and cache the verdict. */
static uint8_t DHT22_CycleCounterReady(void) {
    static uint8_t ready = 2; /* 2 = unprobed, 0 = unusable, 1 = usable */

    if (ready == 2) {
        ready = 0;
        if (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) {
            uint32_t start = DWT->CYCCNT;
            for (volatile uint32_t i = 0; i < 64U; i++) {
                if (DWT->CYCCNT != start) {
                    ready = 1;
                    break;
                }
            }
        }
    }

    return ready;
}

static void DHT22_Delay_us(uint32_t us) {
    if (us == 0U) {
        return;
    }

    uint32_t cycles = DHT22_UsToCycles(us);

    if (DHT22_CycleCounterReady()) {
        uint32_t start = DWT->CYCCNT;
        while ((DWT->CYCCNT - start) < cycles) {
            /* A live counter always advances past `cycles`, so this loop is
               bounded without needing a second timeout. */
        }
    } else {
        /* Fallback: approximate delay using volatile loop, calibrated at
           roughly four cycles per iteration. */
        uint32_t count = cycles / 4U;
        while (count--) { __asm__ volatile("nop"); }
    }
}

/* Wait until the data line reaches `high` (1) or `low` (0).  Every edge of a
   frame follows the previous one within DHT22_EDGE_US, so that constant is the
   bound for all of them.  The cycle budget is compared as an unsigned
   difference of two CYCCNT samples, so a counter wrap reads as a small elapsed
   time rather than a huge one and cannot end the wait early. */
static uint8_t DHT22_WaitLevel(const DHT22_t *dht, uint8_t high) {
    if (DHT22_CycleCounterReady()) {
        uint32_t budget = DHT22_UsToCycles(DHT22_EDGE_US);
        uint32_t start = DWT->CYCCNT;
        for (;;) {
            if (DHT22_IsHigh(dht) == high) {
                return 1U;
            }
            if ((DWT->CYCCNT - start) > budget) {
                return 0U;
            }
        }
    }

    uint32_t guard = DHT22_EDGE_TIMEOUT;
    while (guard-- != 0U) {
        if (DHT22_IsHigh(dht) == high) {
            return 1U;
        }
    }
    return 0U;
}

static uint8_t DHT22_WaitHigh(const DHT22_t *dht) {
    return DHT22_WaitLevel(dht, 1U);
}

static uint8_t DHT22_WaitLow(const DHT22_t *dht) {
    return DHT22_WaitLevel(dht, 0U);
}

static uint8_t DHT22_ComputeChecksum(uint8_t *data) {
    uint8_t crc = 0;
    for (int i = 0; i < 4; i++) {
        crc += data[i];
    }
    return crc;
}

void DHT22_Init(DHT22_t *dht, GPIO_TypeDef *port, uint16_t pin) {
    dht->port = port;
    dht->pin = pin;
    dht->temperature = 0.0f;
    dht->humidity = 0.0f;

    /* MX_GPIO_Init() enables the GPIO port clocks during boot, before this
       driver is initialised.  Unlike HAL_GPIO_Init, the direct CRL/CRH writes
       used here cannot enable a clock themselves, so the precondition is
       documented in dht22.h rather than papered over. */

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DHT22_DWT_CYCCNTENA;

    DHT22_Release(dht);

    /* The sensor needs about 1 s to stabilise after power-up, but blocking
       here would stall the whole boot before the scheduler even starts.
       SensorTask absorbs that settle time with an RTOS delay instead. */
}

/* Drives the start signal and captures the 40 high-pulse widths of one frame.
   The critical section covers only the handshake and the bits: it stops SysTick
   on the patched port, so the millisecond-scale start pulse is produced with
   vTaskDelay before entering it rather than by a busy-wait inside. */
static uint8_t DHT22_CaptureFrame(DHT22_t *dht, uint16_t *high_us) {
    uint8_t measure = DHT22_CycleCounterReady();
    /* Nominal scale, replaced below by one measured from the reply itself. */
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    DHT22_DriveLow(dht);
    vTaskDelay(pdMS_TO_TICKS(DHT22_START_LOW_MS));

    taskENTER_CRITICAL();

    DHT22_Release(dht);

    /* Response signal: the sensor pulls the line low for 80 us, high for 80 us,
       then opens the first bit with a 50 us low pulse.  The 80 us high pulse is
       timed here because its width is fixed: dividing its measured length by 80
       turns the counter's own ticks into microseconds.  Deriving the scale from
       the counter, rather than trusting SystemCoreClock, is what keeps the bit
       decode correct when the two do not agree. */
    if (!DHT22_WaitLow(dht) || !DHT22_WaitHigh(dht)) {
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    if (measure) {
        uint32_t rise = DWT->CYCCNT;
        if (!DHT22_WaitLow(dht)) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
        uint32_t response_cycles = DWT->CYCCNT - rise;
        if (response_cycles >= DHT22_RESPONSE_HIGH_US) {
            cycles_per_us = response_cycles / DHT22_RESPONSE_HIGH_US;
        }
    } else if (!DHT22_WaitLow(dht)) {
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    if (cycles_per_us == 0U) {
        cycles_per_us = 1U;
    }

    for (int i = 0; i < 40; i++) {
        /* Rising edge: end of the 50 us low pulse that opens every bit. */
        if (!DHT22_WaitHigh(dht)) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }

        if (measure) {
            uint32_t rise = DWT->CYCCNT;
            if (!DHT22_WaitLow(dht)) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
            high_us[i] = (uint16_t)((DWT->CYCCNT - rise) / cycles_per_us);
        } else {
            /* Without a cycle counter the width cannot be measured, so sample
               past the 30 us mark and synthesise a width that falls on the
               correct side of the decode threshold. */
            DHT22_Delay_us(30);
            high_us[i] = DHT22_IsHigh(dht) ? (DHT22_ONE_THRESHOLD_US + 1U) : 0U;
            if (!DHT22_WaitLow(dht)) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
        }
    }

    taskEXIT_CRITICAL();

    /* Leave the line released so the sensor can drive the next frame. */
    DHT22_Release(dht);
    return DHT22_OK;
}

uint8_t DHT22_Read(DHT22_t *dht) {
    uint16_t high_us[40];
    uint8_t data[5] = {0};

    uint8_t status = DHT22_CaptureFrame(dht, high_us);
    if (status != DHT22_OK) {
        return status;
    }

    for (int i = 0; i < 40; i++) {
        data[i / 8] <<= 1;
        if (high_us[i] > DHT22_ONE_THRESHOLD_US) {
            data[i / 8] |= 1;
        }
    }

    uint8_t crc = DHT22_ComputeChecksum(data);
    if (crc != data[4]) {
        return DHT22_ERROR;
    }

    for (int i = 0; i < 5; i++) {
        dht->last_data[i] = data[i];
    }

    dht->humidity = (float)((data[0] << 8) | data[1]) / 10.0f;
    dht->temperature = (float)(((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80) {
        dht->temperature = -dht->temperature;
    }

    return DHT22_OK;
}

float DHT22_GetTemperature(const DHT22_t *dht) {
    return dht->temperature;
}

float DHT22_GetHumidity(const DHT22_t *dht) {
    return dht->humidity;
}
