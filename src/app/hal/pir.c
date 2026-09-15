#include "pir.h"

void PIR_Init(PIR_t *pir, GPIO_TypeDef *port, uint16_t pin) {
    pir->port = port;
    pir->pin = pin;
    pir->motion_detected = 0;
    pir->last_trigger_tick = 0;

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

uint8_t PIR_GetState(PIR_t *pir) {
    return pir->motion_detected;
}

void PIR_EXTI_Callback(PIR_t *pir) {
    if (HAL_GPIO_ReadPin(pir->port, pir->pin) == GPIO_PIN_SET) {
        pir->motion_detected = 1;
        pir->last_trigger_tick = HAL_GetTick();
    } else {
        if ((HAL_GetTick() - pir->last_trigger_tick) > 500) {
            pir->motion_detected = 0;
        }
    }
}
