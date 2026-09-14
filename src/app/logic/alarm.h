#ifndef ALARM_H
#define ALARM_H

#include "main.h"
#include "hal/buzzer.h"
#include "logic/temperature.h"

typedef struct {
    Buzzer_t *buzzer;
    uint8_t enabled;
    uint16_t frequency;
    uint32_t last_toggle_tick;
    uint32_t toggle_interval_ms;
    uint8_t alarm_active;
} Alarm_t;

void Alarm_Init(Alarm_t *alarm, Buzzer_t *buzzer);
void Alarm_Update(Alarm_t *alarm, TempStatus_t temp_status);
void Alarm_Enable(Alarm_t *alarm);
void Alarm_Disable(Alarm_t *alarm);
uint8_t Alarm_IsActive(Alarm_t *alarm);

#endif /* ALARM_H */
