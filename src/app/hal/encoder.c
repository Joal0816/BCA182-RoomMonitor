#include "encoder.h"
#include "FreeRTOS.h"
#include "task.h"

void Encoder_Init(Encoder_t *enc,
                  GPIO_TypeDef *clk_port, uint16_t clk_pin,
                  GPIO_TypeDef *dt_port, uint16_t dt_pin,
                  GPIO_TypeDef *sw_port, uint16_t sw_pin) {
    enc->clk_port = clk_port;
    enc->clk_pin = clk_pin;
    enc->dt_port = dt_port;
    enc->dt_pin = dt_pin;
    enc->sw_port = sw_port;
    enc->sw_pin = sw_pin;
    enc->position = 0;
    enc->button_pressed = 0;
    enc->last_clk_state = 0;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = clk_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(clk_port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = dt_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(dt_port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = sw_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(sw_port, &GPIO_InitStruct);

    enc->last_clk_state = HAL_GPIO_ReadPin(clk_port, clk_pin);

    HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);

    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

int8_t Encoder_GetDelta(Encoder_t *enc) {
    taskENTER_CRITICAL();
    int8_t delta = enc->position;
    enc->position = 0;
    taskEXIT_CRITICAL();
    return delta;
}

uint8_t Encoder_IsButtonPressed(Encoder_t *enc) {
    return enc->button_pressed;
}

void Encoder_ClearButton(Encoder_t *enc) {
    enc->button_pressed = 0;
}

void Encoder_CLK_EXTI_Callback(Encoder_t *enc) {
    uint8_t clk_state = HAL_GPIO_ReadPin(enc->clk_port, enc->clk_pin);
    uint8_t dt_state = HAL_GPIO_ReadPin(enc->dt_port, enc->dt_pin);

    if (clk_state == 0) {
        if (dt_state != clk_state) {
            enc->position++;
        } else {
            enc->position--;
        }
    }
}
