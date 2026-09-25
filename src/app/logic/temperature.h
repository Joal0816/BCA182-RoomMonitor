#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdint.h>

/*
 * Temperature limits specified by Laboratory Activity No. 1 (Part 4):
 *   LOW TEMPERATURE LIMIT  = 18 C
 *   HIGH TEMPERATURE LIMIT = 30 C
 *
 * This header is deliberately hardware independent (no HAL / FreeRTOS include)
 * so that the decision logic can be unit tested on the host PC.
 */
#define TEMP_LOW_THRESHOLD   18.0f
#define TEMP_HIGH_THRESHOLD  30.0f

typedef enum {
    TEMP_LOW = 0,
    TEMP_NORMAL,
    TEMP_HIGH
} TempStatus_t;

TempStatus_t EvaluateTemperature(float temperature);
const char* Temperature_GetStatusString(TempStatus_t status);
uint8_t Temperature_IsAlarm(TempStatus_t status);

#endif /* TEMPERATURE_H */
