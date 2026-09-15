#ifndef UART_MUTEX_H
#define UART_MUTEX_H

#include "main.h"

typedef struct {
    UART_HandleTypeDef *huart;
    SemaphoreHandle_t mutex;
} UART_Mutex_t;

void UART_Mutex_Init(UART_Mutex_t *uart_mutex, UART_HandleTypeDef *huart);
void UART_Mutex_Printf(UART_Mutex_t *uart_mutex, const char *format, ...);
void UART_Mutex_Send(UART_Mutex_t *uart_mutex, uint8_t *data, uint16_t size);

#endif /* UART_MUTEX_H */
