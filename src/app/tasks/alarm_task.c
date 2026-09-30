#include "alarm_task.h"
#include <math.h>

void AlarmTask(void *pvParameters) {
    AlarmTaskParams_t *params = (AlarmTaskParams_t *)pvParameters;
    SensorData_t data;

    /* Instrumentation, not production behaviour: latched so the stack margin
       is reported once rather than on every alarm check. */
    static uint8_t margin_reported = 0;

    UART_Mutex_Printf(params->uart_mutex, "[TASK] AlarmTask entered\r\n");

    for (;;) {
        if (xQueueReceive(params->sensor_queue, &data, pdMS_TO_TICKS(ALARM_CHECK_PERIOD_MS)) == pdPASS) {
            StateMachine_Update(params->state_machine, data.motion_detected);

            if (!isnan(data.temperature)) {
                TempStatus_t status = EvaluateTemperature(data.temperature);
                Alarm_Update(params->alarm, status);

                if (Temperature_IsAlarm(status)) {
                    UART_Mutex_Printf(params->uart_mutex,
                                      "[ALARM] Temp=%.1fC Status=%s\r\n",
                                      data.temperature,
                                      Temperature_GetStatusString(status));

                    if (!margin_reported) {
                        margin_reported = 1;
                        /* The margin is scanned only now, after the "%.1f"
                           line above has run.  uxTaskGetStackHighWaterMark()
                           reports the deepest point this task's stack has ever
                           reached -- cumulative over the fill bytes nothing has
                           touched -- so the newlib float formatter and the rest
                           of the alarm path are counted.  Scanning before that
                           line would report the pre-float margin and overstate
                           it by exactly the cost this exists to measure.  The
                           margin line itself formats no float, so measuring it
                           does not deepen the stack after the scan.  Latched to
                           print once, on the first alarm, because the float
                           path is the deepest this task goes. */
                        UART_Mutex_Printf(params->uart_mutex,
                                          "[ALARM] Stack high-water mark: %u words free\r\n",
                                          (unsigned)uxTaskGetStackHighWaterMark(NULL));
                    }
                }
            }
        }
    }
}
