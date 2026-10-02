#include "uart_mutex.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void UART_Mutex_Init(UART_Mutex_t *uart_mutex, UART_HandleTypeDef *huart) {
    uart_mutex->huart = huart;
    uart_mutex->mutex = xSemaphoreCreateMutexStatic(&uart_mutex->mutex_storage);
}

void UART_Mutex_RawSend(UART_Mutex_t *uart_mutex, const uint8_t *data, uint16_t size) {
    USART_TypeDef *instance = uart_mutex->huart->Instance;

    for (uint16_t i = 0; i < size; i++) {
        uint32_t guard = 100000U;
        while (((instance->SR & USART_SR_TXE) == 0U) && (guard-- > 0U)) {
        }
        instance->DR = data[i];
    }
}

void UART_Mutex_Printf(UART_Mutex_t *uart_mutex, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (xSemaphoreTake(uart_mutex->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(uart_mutex->huart, (uint8_t *)buffer, strlen(buffer), 100);
        xSemaphoreGive(uart_mutex->mutex);
    }
}

void UART_Mutex_Send(UART_Mutex_t *uart_mutex, const uint8_t *data, uint16_t size) {
    if (xSemaphoreTake(uart_mutex->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(uart_mutex->huart, data, size, 100);
        xSemaphoreGive(uart_mutex->mutex);
    }
}
