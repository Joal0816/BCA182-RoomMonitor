#include "buzzer.h"

static void Buzzer_SetFrequency(Buzzer_t *buzzer, uint16_t frequency) {
    if (frequency == 0) {
        HAL_TIM_PWM_Stop(buzzer->htim, buzzer->channel);
        return;
    }

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t timer_clock = pclk1;
    if (pclk1 != HAL_RCC_GetHCLKFreq()) {
        timer_clock *= 2;
    }

    uint32_t prescaler = buzzer->htim->Init.Prescaler + 1;
    uint32_t period = (timer_clock / (prescaler * frequency)) - 1;

    __HAL_TIM_SET_AUTORELOAD(buzzer->htim, period);
    __HAL_TIM_SET_COMPARE(buzzer->htim, buzzer->channel, period / 2);
}

void Buzzer_Init(Buzzer_t *buzzer, TIM_HandleTypeDef *htim, uint32_t channel) {
    buzzer->htim = htim;
    buzzer->channel = channel;
    buzzer->playing = 0;
    buzzer->frequency = 0;

    HAL_TIM_PWM_Start(htim, channel);
    __HAL_TIM_SET_COMPARE(htim, channel, 0);
}

uint8_t Buzzer_Play(Buzzer_t *buzzer, uint16_t frequency) {
    if (frequency == 0) {
        Buzzer_Stop(buzzer);
        return BUZZER_OK;
    }

    buzzer->frequency = frequency;
    buzzer->playing = 1;
    Buzzer_SetFrequency(buzzer, frequency);
    __HAL_TIM_SET_COMPARE(buzzer->htim, buzzer->channel,
                          __HAL_TIM_GET_AUTORELOAD(buzzer->htim) / 2);
    return BUZZER_OK;
}

void Buzzer_Stop(Buzzer_t *buzzer) {
    buzzer->playing = 0;
    buzzer->frequency = 0;
    __HAL_TIM_SET_COMPARE(buzzer->htim, buzzer->channel, 0);
}

void Buzzer_Toggle(Buzzer_t *buzzer) {
    if (buzzer->playing) {
        Buzzer_Stop(buzzer);
    } else {
        Buzzer_Play(buzzer, ALARM_FREQ_PATTERN);
    }
}

uint8_t Buzzer_IsPlaying(Buzzer_t *buzzer) {
    return buzzer->playing;
}
