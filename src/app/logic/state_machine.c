#include "state_machine.h"

void StateMachine_Init(StateMachine_t *sm, uint32_t timeout_ms, uint32_t now_ms) {
    sm->current_state = STATE_ACTIVE;
    sm->last_motion_tick_ms = now_ms;
    sm->timeout_ms = timeout_ms;
}

SystemState_t StateMachine_Update(StateMachine_t *sm, uint8_t motion_detected, uint32_t now_ms) {
    if (motion_detected) {
        sm->current_state = STATE_ACTIVE;
        sm->last_motion_tick_ms = now_ms;
        return sm->current_state;
    }

    if (sm->current_state == STATE_ACTIVE) {
        if ((now_ms - sm->last_motion_tick_ms) >= sm->timeout_ms) {
            sm->current_state = STATE_INACTIVE;
        }
    }

    return sm->current_state;
}

SystemState_t StateMachine_GetState(const StateMachine_t *sm) {
    return sm->current_state;
}

void StateMachine_ResetTimeout(StateMachine_t *sm, uint32_t now_ms) {
    sm->last_motion_tick_ms = now_ms;
}
