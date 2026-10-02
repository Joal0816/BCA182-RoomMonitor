#include "state_task.h"

/*
 * StateTask (priority 2) - centralizes ACTIVE / INACTIVE state management.
 *
 * It blocks on the EVENT_MOTION_BIT produced by MotionTask (with a bounded
 * timeout so the inactivity timeout can still be evaluated while no motion
 * events arrive). The resulting state is published through the
 * EVENT_STATE_ACTIVE_BIT, which DisplayTask consumes.
 *
 * This gives the event group a genuine producer/consumer role and keeps the
 * StateMachine_t owned by a single task (no shared-memory race).
 */
void StateTask(void *pvParameters) {
    StateTaskParams_t *params = (StateTaskParams_t *)pvParameters;
    static const char started[] = "[STATE] Task entered\r\n";
    UART_Mutex_RawSend(params->uart_mutex, (const uint8_t *)started,
                       sizeof(started) - 1U);

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(params->event_group,
                                               EVENT_MOTION_BIT,
                                               pdTRUE,   /* clear on exit   */
                                               pdFALSE,  /* wait for any    */
                                               pdMS_TO_TICKS(STATE_POLL_PERIOD_MS));

        uint8_t motion_detected = (bits & EVENT_MOTION_BIT) ? 1 : 0;
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        SystemState_t previous = StateMachine_GetState(params->state_machine);
        SystemState_t current = StateMachine_Update(params->state_machine, motion_detected, now_ms);

        if (current != previous) {
            if (current == STATE_ACTIVE) {
                xEventGroupSetBits(params->event_group, EVENT_STATE_ACTIVE_BIT);
                UART_Mutex_Printf(params->uart_mutex, "[STATE] ACTIVE (motion detected)\r\n");
            } else {
                xEventGroupClearBits(params->event_group, EVENT_STATE_ACTIVE_BIT);
                UART_Mutex_Printf(params->uart_mutex, "[STATE] INACTIVE (%lu s without motion)\r\n",
                                  (unsigned long)(INACTIVE_TIMEOUT_MS / 1000U));
            }
        }
    }
}
