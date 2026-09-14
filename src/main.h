#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#define SYSTEM_CLOCK_HZ  72000000U
#define LED_BLINK_DELAY  500U

typedef struct {
    float temperature;
    float humidity;
    uint16_t light_level;
    uint8_t motion_detected;
    uint32_t timestamp;
} SensorData_t;

typedef enum {
    PAGE_TEMPERATURE = 0,
    PAGE_HUMIDITY,
    PAGE_LIGHT,
    PAGE_MOTION,
    PAGE_COUNT
} DisplayPage_t;

typedef enum {
    STATE_ACTIVE = 0,
    STATE_INACTIVE
} SystemState_t;

typedef enum {
    TEMP_LOW = 0,
    TEMP_NORMAL,
    TEMP_HIGH
} TempStatus_t;

#define MOTION_DETECTED_BIT   (1 << 0)
#define ENCODER_CW_BIT        (1 << 1)
#define ENCODER_CCW_BIT       (1 << 2)
#define ENCODER_BTN_BIT       (1 << 3)

#define TEMP_LOW_THRESHOLD    18.0f
#define TEMP_HIGH_THRESHOLD   30.0f
#define INACTIVE_TIMEOUT_MS   15000U
#define SENSOR_READ_PERIOD_MS 1000U
#define ALARM_CHECK_PERIOD_MS 500U
#define DISPLAY_REFRESH_MS    100U

void Error_Handler(void);

#endif /* MAIN_H */
