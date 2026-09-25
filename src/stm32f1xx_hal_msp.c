/*
 * stm32f1xx_hal_msp.c
 *
 * MCU Support Package (MSP) callbacks for the BCA182 Room Monitoring System.
 *
 * STM32Cube HAL calls these weak functions from each peripheral init routine
 * (HAL_UART_Init, HAL_I2C_Init, HAL_ADC_Init, HAL_TIM_PWM_Init).  They are
 * responsible for enabling the peripheral clock and configuring the GPIO
 * alternate functions.  Without them the HAL writes to unclocked peripheral
 * registers and no data ever leaves the MCU (empty serial monitor, dead OLED,
 * silent buzzer).
 */

#include "main.h"

/**
 * @brief Global MSP initialization, called once by HAL_Init().
 */
void HAL_MspInit(void) {
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/**
 * @brief USART1 MSP initialization: PA9 = TX, PA10 = RX.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 - USART1_TX */
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PA10 - USART1_RX */
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}

/**
 * @brief I2C1 MSP initialization: PB6 = SCL, PB7 = SDA (open-drain AF).
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_I2C1_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    }
}

/**
 * @brief ADC1 MSP initialization: PA0 = ADC1_IN0 (analog, no pull).
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);
    }
}

/**
 * @brief TIM4 PWM MSP initialization: PB8 = TIM4_CH3 (buzzer).
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);
    }
}

/* The generic base-timer MspInit is required by HAL_TIM_Base_Init, which
 * HAL_TIM_PWM_Init does not call, but provide it for completeness so that any
 * future blocking/delay use of TIM4 also has its clock enabled. */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_DISABLE();
    }
}
