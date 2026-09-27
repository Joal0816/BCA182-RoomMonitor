#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portMAX_DELAY 0xFFFFFFFFUL
#define portTICK_PERIOD_MS 1
#define configMAX_PRIORITIES 5
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_MUTEXES 1
#define configTICK_RATE_HZ 1000
#define taskENTER_CRITICAL() do{}while(0)
#define taskEXIT_CRITICAL()  do{}while(0)
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t t);
void vTaskDelayUntil(TickType_t *prev, TickType_t inc);
void taskYIELD(void);
void vApplicationIdleHook(void);
void vTimerCallback(void *ulTimerID);
/* CMSIS intrinsics the driver layer uses */
#ifndef CMSIS_IRQ_STUBS
#define CMSIS_IRQ_STUBS
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
#endif
#endif
