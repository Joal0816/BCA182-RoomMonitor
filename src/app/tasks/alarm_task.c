#include "alarm_task.h"
#include <math.h>

/*
 * AlarmTask (priority 2) - consumes sensor samples and drives the buzzer.
 * The system state machine is handled separately by StateTask.
 */
void AlarmTask(void *pvParameters) {
    AlarmTaskParams_t *params = (AlarmTaskParams_t *)pvParameters;
    SensorData_t data;

    for (;;) {
        if (xQueueReceive(params->alarm_queue, &data, pdMS_TO_TICKS(ALARM_CHECK_PERIOD_MS)) == pdPASS) {
            if (!isnan(data.temperature)) {
                TempStatus_t status = EvaluateTemperature(data.temperature);
                Alarm_Update(params->alarm, status);

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
