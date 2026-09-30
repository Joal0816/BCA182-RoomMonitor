#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "main.h"
#include "hal/oled.h"
#include "logic/state_machine.h"
#include "logic/temperature.h"
#include "drivers/uart_mutex.h"

typedef struct {
    OLED_t *oled;
    /* I2C handle for the panel.  OLED_Init() now runs inside DisplayTask (the
       proven pattern), so the task receives the handle directly instead of
       main() pre-initialising the panel before the scheduler starts. */
    I2C_HandleTypeDef *hi2c;
    QueueHandle_t sensor_queue;
    QueueHandle_t display_page_queue;
    EventGroupHandle_t event_group;
    StateMachine_t *state_machine;
    UART_Mutex_t *uart_mutex;
} DisplayTaskParams_t;

void DisplayTask(void *pvParameters);

#endif /* DISPLAY_TASK_H */
