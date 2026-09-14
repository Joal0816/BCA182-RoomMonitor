#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "main.h"
#include "../hal/dht22.h"
#include "../hal/ldr.h"
#include "../hal/pir.h"
#include "../drivers/uart_mutex.h"

typedef struct {
    DHT22_t *dht22;
    LDR_t *ldr;
    PIR_t *pir;
    QueueHandle_t sensor_queue;
    UART_Mutex_t *uart_mutex;
} SensorTaskParams_t;

void SensorTask(void *pvParameters);

#endif /* SENSOR_TASK_H */
