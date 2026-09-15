#include "alarm_task.h"
#include <math.h>

void AlarmTask(void *pvParameters) {
    AlarmTaskParams_t *params = (AlarmTaskParams_t *)pvParameters;
    SensorData_t data;

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
                }
            }
        }
    }
}
