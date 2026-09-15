#include "state_machine.h"

void StateMachine_Init(StateMachine_t *sm, uint32_t timeout_ms) {
    sm->current_state = STATE_ACTIVE;
    sm->last_motion_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
    sm->timeout_ms = timeout_ms;
}

SystemState_t StateMachine_Update(StateMachine_t *sm, uint8_t motion_detected) {
    uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (motion_detected) {
        sm->current_state = STATE_ACTIVE;
        sm->last_motion_tick = current_tick;
        return sm->current_state;
    }

    if (sm->current_state == STATE_ACTIVE) {
        if ((current_tick - sm->last_motion_tick) >= sm->timeout_ms) {
            sm->current_state = STATE_INACTIVE;
        }
    }

    return sm->current_state;
}

SystemState_t StateMachine_GetState(StateMachine_t *sm) {
    return sm->current_state;
}

void StateMachine_ResetTimeout(StateMachine_t *sm) {
    sm->last_motion_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
}
