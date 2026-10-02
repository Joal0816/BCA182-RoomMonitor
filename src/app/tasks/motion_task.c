#include "motion_task.h"

/*
 * MotionTask (priority 3) - owns the PIR sensor.
 *
 * The PIR EXTI line gives this task a direct notification (event-driven wake).
 * A 500 ms notification timeout is also used as a refresh so that sustained
 * motion keeps EVENT_MOTION_BIT asserted, not just the initial edge.
 *
 * The task blocks in ulTaskNotifyTake() between events; it never busy-waits.
 */
void MotionTask(void *pvParameters) {
    MotionTaskParams_t *params = (MotionTaskParams_t *)pvParameters;
    uint8_t last_reported = 0;
    static const char started[] = "[MOTION] Task entered\r\n";
    UART_Mutex_RawSend(params->uart_mutex, (const uint8_t *)started,
                       sizeof(started) - 1U);

    xEventGroupSetBits(params->event_group, EVENT_MOTION_BIT); /* assume motion present at boot for fast ACTIVE */

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(MOTION_POLL_PERIOD_MS));

        uint8_t motion_state = PIR_GetState(params->pir);

        if (motion_state) {
            xEventGroupSetBits(params->event_group, EVENT_MOTION_BIT);
        } else {
            xEventGroupClearBits(params->event_group, EVENT_MOTION_BIT);
        }

        if (motion_state != last_reported) {
            UART_Mutex_Printf(params->uart_mutex, "[MOTION] State: %s\r\n",
                              motion_state ? "DETECTED" : "CLEAR");
            last_reported = motion_state;
        }
    }
}
