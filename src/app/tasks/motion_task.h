#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#include "main.h"
#include "hal/pir.h"
#include "drivers/uart_mutex.h"

typedef struct {
    PIR_t *pir;
    EventGroupHandle_t event_group;
    UART_Mutex_t *uart_mutex;
    TaskHandle_t task_handle;
} MotionTaskParams_t;

void MotionTask(void *pvParameters);

#endif /* MOTION_TASK_H */
