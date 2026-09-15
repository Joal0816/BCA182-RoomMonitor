#include "temperature.h"

TempStatus_t EvaluateTemperature(float temperature) {
    if (temperature < TEMP_LOW_THRESHOLD) {
        return TEMP_LOW;
    } else if (temperature > TEMP_HIGH_THRESHOLD) {
        return TEMP_HIGH;
    }
    return TEMP_NORMAL;
}

const char* Temperature_GetStatusString(TempStatus_t status) {
    switch (status) {
        case TEMP_LOW:    return "LOW";
        case TEMP_NORMAL: return "NORMAL";
        case TEMP_HIGH:   return "HIGH";
        default:          return "UNKNOWN";
    }
}

uint8_t Temperature_IsAlarm(TempStatus_t status) {
    return (status != TEMP_NORMAL);
}
