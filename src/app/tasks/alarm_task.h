#ifndef ALARM_TASK_H
#define ALARM_TASK_H

#include "main.h"
#include "logic/alarm.h"
#include "logic/temperature.h"
#include "logic/state_machine.h"
#include "drivers/uart_mutex.h"

typedef struct {
    QueueHandle_t sensor_queue;
    Alarm_t *alarm;
    StateMachine_t *state_machine;
    UART_Mutex_t *uart_mutex;
} AlarmTaskParams_t;

void AlarmTask(void *pvParameters);

#endif /* ALARM_TASK_H */
