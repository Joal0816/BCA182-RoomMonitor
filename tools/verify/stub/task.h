#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
/* The real kernel defines TaskHandle_t as a pointer to its TCB; the host stub
   mirrors that (as void *) so NULL is a valid handle, as call sites assume. */
typedef void *TaskHandle_t;
typedef struct {
    const char *pcTaskName;
    void *pxStack;
    TickType_t xTicksToDelay;
} TaskStatus_t;
BaseType_t xTaskCreate(void (*fn)(void*), const char *name, uint16_t stack, void *arg, UBaseType_t prio, TaskHandle_t *handle);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskDelete(TaskHandle_t t);
void vTaskStartScheduler(void);
void vTaskSuspendAll(void);
void xTaskResumeAll(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t w);
BaseType_t xTaskNotifyGive(TaskHandle_t t);
BaseType_t xTaskGetSchedulerState(void);
/* Introspection API used by AlarmTask's runtime instrumentation. */
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
void vTaskNotifyGiveFromISR(TaskHandle_t t, BaseType_t *pxHigherPriorityTaskWoken);
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING     1
#define portYIELD_FROM_ISR(x) do{ (void)(x); }while(0)

#endif

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);
