#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include "logic/temperature.h"
#include "logic/state_machine.h"
#include "logic/display_page.h"

#define SYSTEM_CLOCK_HZ  72000000U
#define LED_BLINK_DELAY  500U

typedef struct {
    float temperature;
    float humidity;
    uint16_t light_level;
    uint8_t motion_detected;
    uint32_t timestamp;
} SensorData_t;

/*
 * Event group bit assignments (Laboratory Activity Part X).
 *
 *   EVENT_MOTION_BIT       Producer: MotionTask. Consumer: StateTask.
 *                          Set while PIR motion is present; StateTask waits on
 *                          this bit (blocking) instead of polling.
 *   EVENT_STATE_ACTIVE_BIT Producer: StateTask (from the state machine).
 *                          Consumer: DisplayTask. Set = ACTIVE, clear = INACTIVE.
 *   EVENT_ALARM_BIT        Producer: AlarmTask. Consumer: diagnostics.
 */
#define EVENT_MOTION_BIT        (1 << 0)
#define EVENT_STATE_ACTIVE_BIT  (1 << 1)
#define EVENT_ALARM_BIT         (1 << 2)

#define INACTIVE_TIMEOUT_MS    15000U
#define SENSOR_READ_PERIOD_MS  1000U
#define ALARM_CHECK_PERIOD_MS  500U
#define DISPLAY_REFRESH_MS     100U
#define STATE_POLL_PERIOD_MS   250U
#define MOTION_POLL_PERIOD_MS  500U
#define QUEUE_SEND_TIMEOUT_MS   50U

void Error_Handler(void);

#endif /* MAIN_H */
