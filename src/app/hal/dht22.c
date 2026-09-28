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

/* Timeout bound for the handshake levels, in microseconds.  This is a bound, not
   an expected value, so it is deliberately generous: the handshake runs before
   the reply has been timed, so it has to be converted with the firmware's own
   idea of the clock, and it must still outlast the 80 us response pulses when
   that idea understates the counter's real rate -- the case on a simulator whose
   core runs at the board's nominal clock regardless of the RCC prescalers the
   firmware programmed.  The worst such case is a nominal 8 MHz against a 72 MHz
   counter, where 1000 * 8 / 72 leaves 111 us, still above 80 us. */
#define DHT22_EDGE_US 1000U

/* Width of the sensor's response high pulse.  It is a fixed part of the
   protocol, so the driver times it and divides by this to learn how many
   counter cycles a microsecond really is. */
#define DHT22_RESPONSE_HIGH_US 80U

/* Bound for the per-bit waits, in microseconds.  The longest level inside a bit
   is the 70 us high pulse; the margin keeps the worst case -- a sensor that
   stalls mid-frame while SysTick is masked -- short.  It is tighter than the
   handshake bound because the bit loop runs 40 times and so dominates that
   window, and it is converted with the rate measured from the reply rather than
   with SystemCoreClock, so it stays that many real microseconds however wrong
   the firmware's idea of the clock is. */
#define DHT22_BIT_EDGE_US 200U

/* Plausible range for the measured scale, in counter cycles per microsecond.  A
   single glitch-stretched response pulse would otherwise inflate the scale and
   bias every bit towards "0", so the measurement is clamped into this range
   before it is used -- for the decode and for the wait bounds alike, because a
   bound tolerates exactly the over-estimate that the clamp allows.  8 covers the
   slowest STM32F1 setting (HSI 8 MHz); 144 covers the fastest plausible core.
   Only a counter above about 400 cycles/us would shorten DHT22_BIT_EDGE_US below
   the 70 us bit pulse, and no STM32F1 core runs anywhere near that. */
#define DHT22_MIN_CYCLES_PER_US 8U
#define DHT22_MAX_CYCLES_PER_US 144U

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
    /* SystemCoreClock is 0 until SystemCoreClockUpdate() has run, and a zero
       result would turn DHT22_Delay_us() into a no-op.  The floor keeps the
       delay non-zero; it cannot make it accurate, because this path only runs
       when the cycle counter is unusable and SystemCoreClock may itself be
       stale.  The caller treats the result as best-effort settling, not timing. */
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us < DHT22_MIN_CYCLES_PER_US) {
        cycles_per_us = DHT22_MIN_CYCLES_PER_US;
    }
    return us * cycles_per_us;
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

/* Wait until the data line reaches `high` (1) or `low` (0), giving up after
   *budget_cycles counter cycles; budget_cycles must not be NULL.  It is passed
   by pointer rather than by value so that this function keeps a single integer
   parameter -- two adjacent integers of convertible type are reported as
   swappable by clang-tidy -- while staying free of shared state, so two callers
   cannot disturb each other's bound.  The budget is compared as an unsigned
   difference of two CYCCNT samples, so a counter wrap reads as a small elapsed
   time rather than a huge one and cannot end the wait early; a counter that had
   stopped, by contrast, would hold elapsed at zero and the wait would not end
   at all, which is why the "counter is running" verdict is cached only once.
   When the DWT cycle counter is not running the budget is unused -- it is
   denominated in DWT cycles, which cannot then be measured -- so the fallback
   counts iterations against the fixed DHT22_EDGE_TIMEOUT instead. */
static uint8_t DHT22_WaitLevel(const DHT22_t *dht, uint8_t high,
                               const uint32_t *budget_cycles) {
    if (DHT22_CycleCounterReady()) {
        uint32_t start = DWT->CYCCNT;
        for (;;) {
            if (DHT22_IsHigh(dht) == high) {
                return 1U;
            }
            if ((DWT->CYCCNT - start) > *budget_cycles) {
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

static uint8_t DHT22_WaitHigh(const DHT22_t *dht, const uint32_t *budget_cycles) {
    return DHT22_WaitLevel(dht, 1U, budget_cycles);
}

static uint8_t DHT22_WaitLow(const DHT22_t *dht, const uint32_t *budget_cycles) {
    return DHT22_WaitLevel(dht, 0U, budget_cycles);
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
    /* Counter cycles per microsecond: the firmware's own idea of it until the
       reply can be measured, floored because SystemCoreClock is 0 until
       SystemCoreClockUpdate() has run.  Everything that needs a microsecond
       budget converts it with this, so the handshake is generous whenever the
       firmware's idea is low and the bit loop becomes exact as soon as the reply
       has replaced it with the measured rate. */
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us < DHT22_MIN_CYCLES_PER_US) {
        cycles_per_us = DHT22_MIN_CYCLES_PER_US;
    }
    uint32_t wait_budget = DHT22_EDGE_US * cycles_per_us;

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
    if (!DHT22_WaitLow(dht, &wait_budget) || !DHT22_WaitHigh(dht, &wait_budget)) {
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    if (measure) {
        uint32_t rise = DWT->CYCCNT;
        if (!DHT22_WaitLow(dht, &wait_budget)) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
        uint32_t measured = (DWT->CYCCNT - rise) / DHT22_RESPONSE_HIGH_US;
        /* Clamp rather than discard: a stretched response over-estimates the
           rate, which biases the decode but only ever makes a bound more
           generous, and clamping leaves the driver with a scale instead of with
           none.  Anything the clamp rejects decodes to a bad checksum, which the
           caller sees as DHT22_ERROR rather than as silent nonsense. */
        cycles_per_us = measured < DHT22_MIN_CYCLES_PER_US ? DHT22_MIN_CYCLES_PER_US
                      : (measured > DHT22_MAX_CYCLES_PER_US ? DHT22_MAX_CYCLES_PER_US
                                                            : measured);
    } else if (!DHT22_WaitLow(dht, &wait_budget)) {
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    /* The reply has replaced the scale with a measured one, so this bound is now
       that many real microseconds. */
    wait_budget = DHT22_BIT_EDGE_US * cycles_per_us;

    for (int i = 0; i < 40; i++) {
        /* Rising edge: end of the 50 us low pulse that opens every bit. */
        if (!DHT22_WaitHigh(dht, &wait_budget)) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }

        if (measure) {
            uint32_t rise = DWT->CYCCNT;
            if (!DHT22_WaitLow(dht, &wait_budget)) {
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
            if (!DHT22_WaitLow(dht, &wait_budget)) {
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
