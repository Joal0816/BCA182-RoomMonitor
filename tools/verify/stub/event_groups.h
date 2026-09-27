#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H
#include "FreeRTOS.h"
typedef struct { void *dummy; } EventGroupHandle_t;
typedef uint32_t EventBits_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t e, EventBits_t b);
EventBits_t xEventGroupClearBits(EventGroupHandle_t e, EventBits_t b);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t e, EventBits_t b, BaseType_t c, BaseType_t a, TickType_t w);
#endif
