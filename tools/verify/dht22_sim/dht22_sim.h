#ifndef DHT22_SIM_H
#define DHT22_SIM_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* Host-side model of a DHT22 on a single GPIO line.
 *
 * The driver (src/app/hal/dht22.c) never calls a HAL read/write helper: it
 * polls GPIO_TypeDef.IDR directly and times pulses with DWT->CYCCNT.  To make
 * that code run on a PC, dht22_sim_prefix.h replaces the DWT that the stub
 * header declares with a macro that advances a *virtual* microsecond clock on
 * every access.  The line level is then a pure function of that clock, so the
 * whole frame is deterministic and needs no wall-clock timing.
 */

typedef enum {
    DHT22_SIM_FRAME = 0, /* reply with the 40-bit frame currently loaded      */
    DHT22_SIM_NONE,      /* line stays high: the sensor never answers          */
    DHT22_SIM_STUCK_LOW, /* line stays low: the reply never rises              */
    DHT22_SIM_TRUNCATED  /* handshake only, then the line idles high           */
} DHT22_SimMode_t;

/* The GPIO instance the simulated sensor hangs off. */
extern GPIO_TypeDef sim_port;

/* CoreDebug register block, so DHT22_Init() does not touch 0xE000EDF0. */
extern CoreDebug_Type dht22_sim_coredebug;

/* Reset the model to idle: pin is the mask of the data line (e.g. GPIO_PIN_1),
 * the mode is DHT22_SIM_FRAME, every bit width is a "0" (26 us) and the line
 * sits high. */
void sim_reset(uint16_t pin);

void sim_set_mode(DHT22_SimMode_t mode);

/* Load the five raw sensor bytes (humidity MSB/LSB, temperature MSB/LSB, CRC)
 * and derive the 40 pulse widths: 70 us for a 1 bit, 26 us for a 0 bit. */
void sim_load_bytes(const uint8_t data[5]);

/* Replace the width of a single bit, to probe the 48 us decode threshold. */
void sim_set_width(int index, uint16_t high_us);

/* Hooks referenced by the injected DWT macro and the simulated RTOS. */
void dht22_sim_tick(void);
DWT_Type *dht22_sim_dwt(void);
void dht22_sim_enter_critical(void);
void dht22_sim_exit_critical(void);

#endif /* DHT22_SIM_H */
