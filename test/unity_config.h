#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#include "stm32f1xx_hal.h"

#define UNITY_OUTPUT_CHAR(c)    HAL_UART_Transmit(&huart1, (uint8_t*)&c, 1, 10)
#define UNITY_OUTPUT_FLUSH()    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 10)

extern UART_HandleTypeDef huart1;

#endif /* UNITY_CONFIG_H */
