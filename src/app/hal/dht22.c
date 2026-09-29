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
   window.  It is converted with the higher of the measured rate and the
   firmware's own, so it is that many real microseconds whenever the reply's
   response pulse measured at least as fast as the firmware believes the clock
   is, and the larger of the two otherwise. */
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
/* Open-drain, not push-pull.  Wokwi's DHT22 model drives the data line itself
   and holds it high while it is idle, so a push-pull low from the MCU leaves two
   push-pull drivers fighting over one wire; the model then reads an undefined
   level rather than a start pulse, never replies, and every read times out
   (status 2).  An open-drain low only sinks, which resolves to a clean low.  It
   needs no pull-up here because the start pulse only ever drives the line down;
   the release below supplies the high. */
#define DHT22_CNF_OUT_OD_2MHZ 0x6U /* output open-drain, 2 MHz */

#define DHT22_DWT_CYCCNTENA (1UL << 0UL)

/* TEMPORARY DIAGNOSTIC -- remove before committing.  Every read fails with
   DHT22_TIMEOUT, which does not say which wait expired nor what the line was
   doing when it did.  These record the two levels that separate "the line never
   rises" from "the sensor never answers", the wait that failed, and whether the
   DWT path was taken at all.

   Stage: 0 = handshake passed, 1 = response low, 2 = response high, 3 = first
   bit, 4 = the line never returned to idle high, 5 = a bit's rising edge,
   6 = a bit's falling edge.  The bit loop sets a stage too: without that, a
   frame that dies part-way through would print stage 0 and look exactly like a
   handshake that passed. */
volatile uint8_t DHT22_DiagIdle;     /* IDR before the start pulse */
volatile uint8_t DHT22_DiagReleased; /* IDR straight after DHT22_Release */
volatile uint8_t DHT22_DiagStage;    /* 0 = handshake passed, else the wait that failed */
volatile uint8_t DHT22_DiagFinal;    /* IDR when that wait gave up */
volatile uint8_t DHT22_DiagBit;      /* bit index when stage is 5 or 6 */
volatile uint8_t DHT22_DiagDwt;      /* DHT22_CycleCounterReady() verdict */

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
    DHT22_SetConfig(dht, DHT22_CNF_OUT_OD_2MHZ);
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
    /* Counter cycles per microsecond as the firmware believes it, floored
       because SystemCoreClock is 0 until SystemCoreClockUpdate() has run. */
    uint32_t nominal_scale = SystemCoreClock / 1000000U;
    if (nominal_scale < DHT22_MIN_CYCLES_PER_US) {
        nominal_scale = DHT22_MIN_CYCLES_PER_US;
    }
    /* cycles_per_us is the decode scale, replaced below by the rate measured
       from the reply.  bound_scale is what the waits are budgeted against, and it
       must not under-estimate the rate: a bound that is too generous only costs
       stall time on the failure path, while one that is too small ends a wait the
       sensor was still about to satisfy.  The handshake has nothing but the
       firmware's belief to go on, and neither has the bit loop in the one corner
       where the response pulse measured short and the firmware also under-states
       the clock: no evidence available here yields a rate above the real one, so
       a bad frame is then reported as a timeout rather than as DHT22_ERROR. */
    uint32_t cycles_per_us = nominal_scale;
    uint32_t bound_scale = nominal_scale;
    uint32_t wait_budget = DHT22_EDGE_US * bound_scale;

    DHT22_DiagDwt = measure;
    DHT22_DiagIdle = DHT22_IsHigh(dht);

    DHT22_DriveLow(dht);
    vTaskDelay(pdMS_TO_TICKS(DHT22_START_LOW_MS));

    taskENTER_CRITICAL();

    DHT22_Release(dht);
    DHT22_DiagReleased = DHT22_IsHigh(dht);
    DHT22_DiagStage = 0;
    DHT22_DiagFinal = 0;

    /* The line idles high once the host lets go of it; the sensor answers 20-40
       us later with its 80 us low.  Waiting for that idle high first is what
       keeps a release that has not taken effect yet -- the line still low from
       the start pulse -- from being mistaken for the reply.  Without it the
       response-low wait returns immediately, every later level is then read one
       edge early, and the frame runs off its end and times out. */
    if (!DHT22_WaitHigh(dht, &wait_budget)) {
        DHT22_DiagStage = 4;
        DHT22_DiagFinal = DHT22_IsHigh(dht);
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    /* Response signal: the sensor pulls the line low for 80 us, high for 80 us,
       then opens the first bit with a 50 us low pulse.  The 80 us high pulse is
       timed here because its width is fixed: dividing its measured length by 80
       turns the counter's own ticks into microseconds.  Deriving the scale from
       the counter, rather than trusting SystemCoreClock, is what keeps the bit
       decode correct when the two do not agree. */
    if (!DHT22_WaitLow(dht, &wait_budget)) {
        DHT22_DiagStage = 1;
        DHT22_DiagFinal = DHT22_IsHigh(dht);
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }
    if (!DHT22_WaitHigh(dht, &wait_budget)) {
        DHT22_DiagStage = 2;
        DHT22_DiagFinal = DHT22_IsHigh(dht);
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    if (measure) {
        uint32_t rise = DWT->CYCCNT;
        if (!DHT22_WaitLow(dht, &wait_budget)) {
            DHT22_DiagStage = 3;
            DHT22_DiagFinal = DHT22_IsHigh(dht);
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
        uint32_t measured = (DWT->CYCCNT - rise) / DHT22_RESPONSE_HIGH_US;
        /* Clamp rather than discard: the measurement is still the best evidence
           of the rate, and a response pulse that measures outside the plausible
           band decodes to a bad checksum, which the caller sees as DHT22_ERROR
           rather than as silent nonsense. */
        cycles_per_us = measured < DHT22_MIN_CYCLES_PER_US ? DHT22_MIN_CYCLES_PER_US
                      : (measured > DHT22_MAX_CYCLES_PER_US ? DHT22_MAX_CYCLES_PER_US
                                                            : measured);
        /* The bound takes the higher of the two.  A response pulse that measures
           short makes the measurement under-estimate the rate, which is exactly
           the direction a bound must not follow. */
        if (cycles_per_us > bound_scale) {
            bound_scale = cycles_per_us;
        }
    } else if (!DHT22_WaitLow(dht, &wait_budget)) {
        DHT22_DiagStage = 3;
        DHT22_DiagFinal = DHT22_IsHigh(dht);
        taskEXIT_CRITICAL();
        return DHT22_TIMEOUT;
    }

    /* The reply has supplied a measured rate and bound_scale is the higher of it
       and the firmware's belief, so this is DHT22_BIT_EDGE_US real microseconds
       whenever either input is at least the real rate -- the corner noted above
       and a measurement clamped at the cap being the exceptions -- and generously
       longer when the firmware over-estimates the clock. */
    wait_budget = DHT22_BIT_EDGE_US * bound_scale;

    for (int i = 0; i < 40; i++) {
        /* Rising edge: end of the 50 us low pulse that opens every bit. */
        if (!DHT22_WaitHigh(dht, &wait_budget)) {
            DHT22_DiagStage = 5;
            DHT22_DiagBit = (uint8_t)i;
            DHT22_DiagFinal = DHT22_IsHigh(dht);
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }

        if (measure) {
            uint32_t rise = DWT->CYCCNT;
            if (!DHT22_WaitLow(dht, &wait_budget)) {
                DHT22_DiagStage = 6;
                DHT22_DiagBit = (uint8_t)i;
                DHT22_DiagFinal = DHT22_IsHigh(dht);
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
                DHT22_DiagStage = 6;
                DHT22_DiagBit = (uint8_t)i;
                DHT22_DiagFinal = DHT22_IsHigh(dht);
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
