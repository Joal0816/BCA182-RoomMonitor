#include "dht22_sim.h"
#include "FreeRTOS.h"
#include <string.h>

GPIO_TypeDef sim_port;
CoreDebug_Type dht22_sim_coredebug;
uint32_t SystemCoreClock = 72000000U;

static DWT_Type g_dwt;
static uint16_t g_width[40];
static uint16_t g_pin;
static uint32_t g_us;
static DHT22_SimMode_t g_mode;

/* Counter cycles per virtual microsecond.  Normally this follows
 * SystemCoreClock, but a test can move it independently to model a core whose
 * clock the firmware has mis-identified (Wokwi runs the core at the board's
 * nominal rate whatever RCC prescalers the firmware programmed). */
static uint32_t g_counter_cycles_per_us = 72U;

/* Pulse shapes of one DHT22 frame, in microseconds.  The sensor answers the
 * start signal with an 80 us low then an 80 us high pulse; every data bit is a
 * 50 us low pulse followed by a high pulse whose width encodes the bit. */
#define DHT22_SIM_RESPONSE_LOW_US  80U
#define DHT22_SIM_RESPONSE_HIGH_US 80U
#define DHT22_SIM_BIT_LOW_US       50U

/* How long the line takes to come back up after the host releases it.  A real
 * wire is charged by the pull-up over a microsecond or two, so it is still low
 * for a moment after the release.  Modelling that is what gives the driver's
 * idle-high wait something to do: with the line already high at the release the
 * wait is a no-op, and the model cannot tell the fixed driver apart from one
 * that goes straight to waiting for the reply.
 *
 * This is load-bearing, not decoration.  Verified by disabling the leading wait
 * in dht22.c: 28 sim assertions then fail.  Without the rise window that
 * experiment would pass, because every edge would still land where the broken
 * driver expects it.  Do not "simplify" this back to a high line at t = 0. */
#define DHT22_SIM_RISE_US          2U

/* How long the line then idles high before the sensor answers.  The datasheet
 * gives 20-40 us, and the driver waits for this idle high, so modelling it is
 * what lets a test tell the release apart from the reply. */
#define DHT22_SIM_IDLE_HIGH_US     30U

/* The response high pulse is the one the driver times to learn its scale, so a
 * test can stretch or shorten it to drive the driver's plausibility clamp. */
static uint16_t g_response_high_us = DHT22_SIM_RESPONSE_HIGH_US;

static uint8_t dht22_sim_level(uint32_t t) {
    if (g_mode == DHT22_SIM_NONE) {
        return 1U;
    }
    if (g_mode == DHT22_SIM_STUCK_LOW) {
        return 0U;
    }

    if (t < DHT22_SIM_RISE_US) {
        return 0U;
    }
    t -= DHT22_SIM_RISE_US;

    if (t < DHT22_SIM_IDLE_HIGH_US) {
        return 1U;
    }
    t -= DHT22_SIM_IDLE_HIGH_US;

    if (t < DHT22_SIM_RESPONSE_LOW_US) {
        return 0U;
    }
    t -= DHT22_SIM_RESPONSE_LOW_US;

    if (t < g_response_high_us) {
        return 1U;
    }
    t -= g_response_high_us;

    if (g_mode == DHT22_SIM_TRUNCATED) {
        return 1U;
    }

    for (int i = 0; i < 40; i++) {
        if (t < DHT22_SIM_BIT_LOW_US) {
            return 0U;
        }
        t -= DHT22_SIM_BIT_LOW_US;

        if (t < g_width[i]) {
            return 1U;
        }
        t -= g_width[i];
    }

    /* The sensor closes the frame with one more 50 us low before it lets the
     * pull-up return the line high; the driver's per-bit "wait for the falling
     * edge" relies on it after the last bit. */
    if (t < DHT22_SIM_BIT_LOW_US) {
        return 0U;
    }

    return 1U; /* idle high until the next frame */
}

void dht22_sim_tick(void) {
    g_us++;
    g_dwt.CYCCNT = g_us * g_counter_cycles_per_us;
    sim_port.IDR = dht22_sim_level(g_us) ? (uint32_t)g_pin : 0U;
}

DWT_Type *dht22_sim_dwt(void) {
    return &g_dwt;
}

void dht22_sim_enter_critical(void) {
    g_us = 0U;
    sim_port.IDR = dht22_sim_level(0U) ? (uint32_t)g_pin : 0U;
}

void dht22_sim_exit_critical(void) {
}

/* The driver does all of its own timing, so the RTOS delay is a no-op here. */
void vTaskDelay(TickType_t ticks) {
    (void)ticks;
}

void sim_reset(uint16_t pin) {
    g_pin = pin;
    g_us = 0U;
    g_mode = DHT22_SIM_FRAME;
    g_counter_cycles_per_us = SystemCoreClock / 1000000U;
    g_response_high_us = DHT22_SIM_RESPONSE_HIGH_US;
    g_dwt.CTRL = 0U;
    g_dwt.CYCCNT = 0U;

    for (int i = 0; i < 40; i++) {
        g_width[i] = 26U;
    }

    memset(&sim_port, 0, sizeof(sim_port));
    sim_port.IDR = pin;
}

void sim_set_mode(DHT22_SimMode_t mode) {
    g_mode = mode;
}

void sim_set_width(int index, uint16_t high_us) {
    if (index >= 0 && index < 40) {
        g_width[index] = high_us;
    }
}

void sim_load_bytes(const uint8_t data[5]) {
    g_mode = DHT22_SIM_FRAME;

    for (int i = 0; i < 40; i++) {
        uint8_t byte = data[i / 8];
        uint8_t bit = (uint8_t)((byte >> (7 - (i % 8))) & 0x1U);
        g_width[i] = bit ? 70U : 26U;
    }
}

void sim_set_counter_cycles_per_us(uint32_t cycles_per_us) {
    g_counter_cycles_per_us = cycles_per_us;
}

/* Takes effect at the next frame: the level schedule is a function of the frame
 * time, so changing the width while a frame is in flight desynchronises it. */
void sim_set_response_high_us(uint16_t us) {
    g_response_high_us = us;
}
