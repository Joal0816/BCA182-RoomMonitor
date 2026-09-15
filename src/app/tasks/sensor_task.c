#include "sensor_task.h"
#include <math.h>

void SensorTask(void *pvParameters) {
    SensorTaskParams_t *params = (SensorTaskParams_t *)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS);

    for (;;) {
        SensorData_t data;
        data.timestamp = xTaskGetTickCount();
        uint8_t read_ok = 1;

        if (DHT22_Read(params->dht22) == DHT22_OK) {
            data.temperature = DHT22_GetTemperature(params->dht22);
            data.humidity = DHT22_GetHumidity(params->dht22);
        } else {
            data.temperature = NAN;
            data.humidity = NAN;
            read_ok = 0;
            UART_Mutex_Printf(params->uart_mutex, "[SENSOR] DHT22 read error\r\n");
        }

        if (LDR_Read(params->ldr) == LDR_OK) {
            data.light_level = LDR_GetValue(params->ldr);
        } else {
            data.light_level = 0;
            UART_Mutex_Printf(params->uart_mutex, "[SENSOR] LDR read error\r\n");
        }

        data.motion_detected = PIR_GetState(params->pir);

        if (read_ok) {
            xQueueSend(params->sensor_queue, &data, 0);
        }

        UART_Mutex_Printf(params->uart_mutex,
                         "[SENSOR] T=%.1fC H=%.1f%% L=%d M=%d\r\n",
                         data.temperature, data.humidity,
                         data.light_level, data.motion_detected);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
