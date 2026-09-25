#ifndef STATE_TASK_H
#define STATE_TASK_H

#include "main.h"
#include "logic/state_machine.h"
#include "drivers/uart_mutex.h"

typedef struct {
    StateMachine_t *state_machine;
    EventGroupHandle_t event_group;
    UART_Mutex_t *uart_mutex;
} StateTaskParams_t;

void StateTask(void *pvParameters);

#endif /* STATE_TASK_H */
