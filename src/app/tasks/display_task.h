#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "main.h"
#include "hal/oled.h"
#include "logic/temperature.h"
#include "drivers/uart_mutex.h"

typedef struct {
    OLED_t *oled;
    QueueHandle_t display_queue;       /* SensorData_t   from SensorTask  */
    QueueHandle_t display_page_queue;  /* DisplayPage_t  from InputTask   */
    EventGroupHandle_t event_group;    /* EVENT_STATE_ACTIVE_BIT          */
    UART_Mutex_t *uart_mutex;
} DisplayTaskParams_t;

void DisplayTask(void *pvParameters);

#endif /* DISPLAY_TASK_H */
