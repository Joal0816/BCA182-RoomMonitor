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

#define DHT22_SIM_CYCLES_PER_US (SystemCoreClock / 1000000U)

/* Pulse shapes of one DHT22 frame, in microseconds.  The sensor answers the
 * start signal with an 80 us low then an 80 us high pulse; every data bit is a
 * 50 us low pulse followed by a high pulse whose width encodes the bit. */
#define DHT22_SIM_RESPONSE_LOW_US  80U
#define DHT22_SIM_RESPONSE_HIGH_US 80U
#define DHT22_SIM_BIT_LOW_US       50U

static uint8_t dht22_sim_level(uint32_t t) {
    if (g_mode == DHT22_SIM_NONE) {
        return 1U;
    }
    if (g_mode == DHT22_SIM_STUCK_LOW) {
        return 0U;
    }

    if (t < DHT22_SIM_RESPONSE_LOW_US) {
        return 0U;
    }
    t -= DHT22_SIM_RESPONSE_LOW_US;

    if (t < DHT22_SIM_RESPONSE_HIGH_US) {
        return 1U;
    }
    t -= DHT22_SIM_RESPONSE_HIGH_US;

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
    g_dwt.CYCCNT = g_us * DHT22_SIM_CYCLES_PER_US;
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
