#ifndef LDR_H
#define LDR_H

#include "stm32f1xx_hal.h"

#define LDR_OK       0
#define LDR_ERROR    1

typedef struct {
    ADC_HandleTypeDef *hadc;
    uint32_t channel;
    uint16_t last_value;
} LDR_t;

void LDR_Init(LDR_t *ldr, ADC_HandleTypeDef *hadc, uint32_t channel);
uint8_t LDR_Read(LDR_t *ldr);
uint16_t LDR_GetValue(LDR_t *ldr);

#endif /* LDR_H */
