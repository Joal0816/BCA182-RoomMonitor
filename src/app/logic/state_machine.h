#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "main.h"

typedef struct {
    SystemState_t current_state;
    uint32_t last_motion_tick;
    uint32_t timeout_ms;
} StateMachine_t;

void StateMachine_Init(StateMachine_t *sm, uint32_t timeout_ms);
SystemState_t StateMachine_Update(StateMachine_t *sm, uint8_t motion_detected);
SystemState_t StateMachine_GetState(StateMachine_t *sm);
void StateMachine_ResetTimeout(StateMachine_t *sm);

#endif /* STATE_MACHINE_H */
