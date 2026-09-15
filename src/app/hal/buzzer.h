#ifndef BUZZER_H
#define BUZZER_H

#include "stm32f1xx_hal.h"

#define BUZZER_OK       0
#define BUZZER_ERROR    1

#define ALARM_FREQ_LOW     1000
#define ALARM_FREQ_HIGH    2000
#define ALARM_FREQ_PATTERN 1500

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t playing;
    uint16_t frequency;
} Buzzer_t;

void Buzzer_Init(Buzzer_t *buzzer, TIM_HandleTypeDef *htim, uint32_t channel);
uint8_t Buzzer_Play(Buzzer_t *buzzer, uint16_t frequency);
void Buzzer_Stop(Buzzer_t *buzzer);
void Buzzer_Toggle(Buzzer_t *buzzer);
uint8_t Buzzer_IsPlaying(Buzzer_t *buzzer);

#endif /* BUZZER_H */
