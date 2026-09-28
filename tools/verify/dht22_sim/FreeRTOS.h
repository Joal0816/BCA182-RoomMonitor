#ifndef FREERTOS_H
#define FREERTOS_H

/* Minimal FreeRTOS surface for the DHT22 host simulation.  dht22.c only uses
 * vTaskDelay(), pdMS_TO_TICKS() and the two critical-section macros, so this
 * shim declares exactly those and routes the critical section into the sensor
 * model, where it marks the start of a frame. */

#include <stdint.h>
#include "dht22_sim.h"

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(x) ((TickType_t)(x))

void vTaskDelay(TickType_t ticks);

#define taskENTER_CRITICAL() dht22_sim_enter_critical()
#define taskEXIT_CRITICAL()  dht22_sim_exit_critical()

#endif /* FREERTOS_H */
