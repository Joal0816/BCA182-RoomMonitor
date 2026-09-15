#include "motion_task.h"

void MotionTask(void *pvParameters) {
    MotionTaskParams_t *params = (MotionTaskParams_t *)pvParameters;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint8_t motion_state = PIR_GetState(params->pir);

        if (motion_state) {
            xEventGroupSetBits(params->event_group, MOTION_DETECTED_BIT);
        } else {
            xEventGroupClearBits(params->event_group, MOTION_DETECTED_BIT);
        }

        UART_Mutex_Printf(params->uart_mutex, "[MOTION] State: %s\r\n",
                         motion_state ? "DETECTED" : "CLEAR");
    }
}
