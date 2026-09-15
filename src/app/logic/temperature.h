#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include "main.h"

TempStatus_t EvaluateTemperature(float temperature);
const char* Temperature_GetStatusString(TempStatus_t status);
uint8_t Temperature_IsAlarm(TempStatus_t status);

#endif /* TEMPERATURE_H */
