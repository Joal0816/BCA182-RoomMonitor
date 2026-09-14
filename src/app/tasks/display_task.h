#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "main.h"
#include "hal/oled.h"
#include "logic/state_machine.h"
#include "logic/temperature.h"
#include "drivers/uart_mutex.h"

typedef struct {
    OLED_t *oled;
    QueueHandle_t sensor_queue;
    QueueHandle_t display_page_queue;
    StateMachine_t *state_machine;
    UART_Mutex_t *uart_mutex;
} DisplayTaskParams_t;

void DisplayTask(void *pvParameters);

#endif /* DISPLAY_TASK_H */
