#ifndef QUEUE_H
#define QUEUE_H
#include "FreeRTOS.h"
typedef struct { void *dummy; } QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t size);
BaseType_t xQueueSend(QueueHandle_t q, const void *i, TickType_t w);
BaseType_t xQueueOverwrite(QueueHandle_t q, const void *i);
BaseType_t xQueueReceive(QueueHandle_t q, void *o, TickType_t w);
#endif
