#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

/*
 * Hardware-independent ACTIVE / INACTIVE state machine.
 *
 * The current time is injected by the caller (now_ms) instead of being read
 * from FreeRTOS, so this module can be exercised by host unit tests.
 */

typedef enum {
    STATE_ACTIVE = 0,
    STATE_INACTIVE
} SystemState_t;

typedef struct {
    SystemState_t current_state;
    uint32_t last_motion_tick_ms;
    uint32_t timeout_ms;
} StateMachine_t;

void StateMachine_Init(StateMachine_t *sm, uint32_t timeout_ms, uint32_t now_ms);
SystemState_t StateMachine_Update(StateMachine_t *sm, uint8_t motion_detected, uint32_t now_ms);
SystemState_t StateMachine_GetState(const StateMachine_t *sm);
void StateMachine_ResetTimeout(StateMachine_t *sm, uint32_t now_ms);

#endif /* STATE_MACHINE_H */
