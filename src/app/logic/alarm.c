#include "alarm.h"

void Alarm_Init(Alarm_t *alarm, Buzzer_t *buzzer) {
    alarm->buzzer = buzzer;
    alarm->enabled = 1;
    alarm->frequency = ALARM_FREQ_PATTERN;
    alarm->last_toggle_tick = 0;
    alarm->toggle_interval_ms = 500;
    alarm->alarm_active = 0;
}

void Alarm_Update(Alarm_t *alarm, TempStatus_t temp_status) {
    if (!alarm->enabled) {
        if (alarm->alarm_active) {
            Buzzer_Stop(alarm->buzzer);
            alarm->alarm_active = 0;
        }
        return;
    }

    if (Temperature_IsAlarm(temp_status)) {
        uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (!alarm->alarm_active) {
            alarm->alarm_active = 1;
            alarm->last_toggle_tick = current_tick;

            if (temp_status == TEMP_LOW) {
                Buzzer_Play(alarm->buzzer, ALARM_FREQ_LOW);
            } else {
                Buzzer_Play(alarm->buzzer, ALARM_FREQ_HIGH);
            }
        } else {
            if ((current_tick - alarm->last_toggle_tick) >= alarm->toggle_interval_ms) {
                Buzzer_Toggle(alarm->buzzer);
                alarm->last_toggle_tick = current_tick;
            }
        }
    } else {
        if (alarm->alarm_active) {
            Buzzer_Stop(alarm->buzzer);
            alarm->alarm_active = 0;
        }
    }
}

void Alarm_Enable(Alarm_t *alarm) {
    alarm->enabled = 1;
}

void Alarm_Disable(Alarm_t *alarm) {
    alarm->enabled = 0;
    Buzzer_Stop(alarm->buzzer);
    alarm->alarm_active = 0;
}

uint8_t Alarm_IsActive(Alarm_t *alarm) {
    return alarm->alarm_active;
}
