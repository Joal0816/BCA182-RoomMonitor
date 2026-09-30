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

                if (!margin_reported) {
                    margin_reported = 1;
                    /* uxTaskGetStackHighWaterMark() returns the words of stack
                       this task never touched, i.e. the margin left after the
                       float-formatter path above has run.  Printed once, on
                       the first successful evaluation, so a rerun proves the
                       margin exists instead of merely that the overflow hook
                       stayed silent. */
                    UART_Mutex_Printf(params->uart_mutex,
                                      "[ALARM] Stack high-water mark: %u words free (at %.1fC)\r\n",
                                      (unsigned)uxTaskGetStackHighWaterMark(NULL),
                                      data.temperature);
                }

                if (Temperature_IsAlarm(status)) {
                    UART_Mutex_Printf(params->uart_mutex,
                                     "[ALARM] Temp=%.1fC Status=%s\r\n",
                                     data.temperature,
                                     Temperature_GetStatusString(status));
                }
            }
        }
    }
}
