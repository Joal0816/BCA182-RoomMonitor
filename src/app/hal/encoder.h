#ifndef ENCODER_H
#define ENCODER_H

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *clk_port;
    uint16_t clk_pin;
    GPIO_TypeDef *dt_port;
    uint16_t dt_pin;
    GPIO_TypeDef *sw_port;
    uint16_t sw_pin;
    volatile int8_t position;
    volatile uint8_t button_pressed;
    uint8_t last_clk_state;
} Encoder_t;

void Encoder_Init(Encoder_t *enc,
                  GPIO_TypeDef *clk_port, uint16_t clk_pin,
                  GPIO_TypeDef *dt_port, uint16_t dt_pin,
                  GPIO_TypeDef *sw_port, uint16_t sw_pin);
int8_t Encoder_GetDelta(Encoder_t *enc);
uint8_t Encoder_IsButtonPressed(Encoder_t *enc);
void Encoder_ClearButton(Encoder_t *enc);
void Encoder_CLK_EXTI_Callback(Encoder_t *enc);

#endif /* ENCODER_H */
