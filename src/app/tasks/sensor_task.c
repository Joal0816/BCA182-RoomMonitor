#include "sensor_task.h"
#include <math.h>

void SensorTask(void *pvParameters) {
    SensorTaskParams_t *params = (SensorTaskParams_t *)pvParameters;
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS);

    /* Printed before the settle delay, so the log shows whether the task was
       even scheduled during the first second of the run. */
    UART_Mutex_Printf(params->uart_mutex, "[TASK] SensorTask entered\r\n");

    /* DHT22_Init hands the sensor's ~1 s power-up settling time to this task
       so boot is not blocked.  Waiting here means the first read happens on a
       settled sensor, rather than failing at t=0 and leaving both queues
       empty until the next period. */
    vTaskDelay(pdMS_TO_TICKS(SENSOR_SETTLE_MS));

    /* Taken after the settle delay.  xLastWakeTime must not be a full period
       stale, or vTaskDelayUntil() finds its deadline already passed on the
       first iteration and returns immediately -- issuing two reads back to
       back instead of one per period. */
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        SensorData_t data;
        data.timestamp = xTaskGetTickCount();
        uint8_t read_ok = 1;

        uint8_t dht_status = DHT22_Read(params->dht22);
        if (dht_status == DHT22_OK) {
            data.temperature = DHT22_GetTemperature(params->dht22);
            data.humidity = DHT22_GetHumidity(params->dht22);
        } else {
            data.temperature = NAN;
            data.humidity = NAN;
            read_ok = 0;
            /* The status separates a silent sensor (TIMEOUT) from a reply that
               arrived but did not check out (ERROR) -- the first thing worth
               knowing when a read fails in the simulator. */
            UART_Mutex_Printf(params->uart_mutex,
                             "[SENSOR] DHT22 read error (status=%u)\r\n",
                             (unsigned)dht_status);
            /* TEMPORARY DIAGNOSTIC -- remove before committing.  idle/released
               are the line levels either side of the start pulse, stage is the
               handshake wait that expired (1 = response low, 2 = response high,
               3 = first bit), final is the level it gave up on, dwt is whether
               the cycle-counter path was taken. */
            UART_Mutex_Printf(params->uart_mutex,
                             "[DHTDIAG] idle=%u released=%u stage=%u final=%u dwt=%u\r\n",
                             (unsigned)DHT22_DiagIdle,
                             (unsigned)DHT22_DiagReleased,
                             (unsigned)DHT22_DiagStage,
                             (unsigned)DHT22_DiagFinal,
                             (unsigned)DHT22_DiagDwt);
        }

        if (LDR_Read(params->ldr) == LDR_OK) {
            data.light_level = LDR_GetValue(params->ldr);
        } else {
            data.light_level = 0;
            UART_Mutex_Printf(params->uart_mutex, "[SENSOR] LDR read error\r\n");
        }

        data.motion_detected = PIR_GetState(params->pir);

        /* Publish the sample independently so AlarmTask and DisplayTask do
           not compete for the same queue item.  A length-one queue keeps the
           latest sample when a consumer is busy rendering or alarming. */
        if (read_ok) {
            xQueueOverwrite(params->alarm_queue, &data);
            xQueueOverwrite(params->display_queue, &data);
        }

        UART_Mutex_Printf(params->uart_mutex,
                         "[SENSOR] T=%.1fC H=%.1f%% L=%d M=%d\r\n",
                         data.temperature, data.humidity,
                         data.light_level, data.motion_detected);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
