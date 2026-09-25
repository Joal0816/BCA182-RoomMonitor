#ifndef INPUT_TASK_H
#define INPUT_TASK_H

#include "main.h"
#include "hal/encoder.h"
#include "drivers/uart_mutex.h"

typedef struct {
    Encoder_t *encoder;
    QueueHandle_t display_queue;   /* DisplayPage_t -> DisplayTask */
    UART_Mutex_t *uart_mutex;
    TaskHandle_t task_handle;
} InputTaskParams_t;

void InputTask(void *pvParameters);

#endif /* INPUT_TASK_H */
