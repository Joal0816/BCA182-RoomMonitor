#include "ldr.h"

void LDR_Init(LDR_t *ldr, ADC_HandleTypeDef *hadc, uint32_t channel) {
    ldr->hadc = hadc;
    ldr->channel = channel;
    ldr->last_value = 0;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(hadc, &sConfig);
}

uint8_t LDR_Read(LDR_t *ldr) {
    HAL_ADC_Start(ldr->hadc);
    if (HAL_ADC_PollForConversion(ldr->hadc, 10) == HAL_OK) {
        ldr->last_value = (uint16_t)HAL_ADC_GetValue(ldr->hadc);
        HAL_ADC_Stop(ldr->hadc);
        return LDR_OK;
    }
    HAL_ADC_Stop(ldr->hadc);
    return LDR_ERROR;
}

uint16_t LDR_GetValue(LDR_t *ldr) {
    return ldr->last_value;
}
