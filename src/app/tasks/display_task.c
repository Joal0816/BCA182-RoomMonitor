#include "display_task.h"
#include <stdio.h>
#include <string.h>

static void DrawHeader(OLED_t *oled, const char *title) {
    OLED_DrawRect(oled, 0, 0, OLED_WIDTH, 12, 1);
    OLED_DrawString(oled, 4, 3, title, 1);
}

static void DrawStatusBar(OLED_t *oled, SystemState_t state) {
    const char *state_str = (state == STATE_ACTIVE) ? "ACTIVE" : "INACTIVE";
    OLED_DrawString(oled, OLED_WIDTH - 48, 3, state_str, 1);
}

static void DrawTemperaturePage(OLED_t *oled, SensorData_t *data) {
    char buf[32];
    DrawHeader(oled, "TEMPERATURE");

    snprintf(buf, sizeof(buf), "%.1f C", data->temperature);
    OLED_DrawString(oled, 10, 20, buf, 2);

    TempStatus_t status = EvaluateTemperature(data->temperature);
    const char *status_str = Temperature_GetStatusString(status);
    OLED_DrawString(oled, 10, 45, "Status: ", 1);
    OLED_DrawString(oled, 58, 45, status_str, 1);
}

static void DrawHumidityPage(OLED_t *oled, SensorData_t *data) {
    char buf[32];
    DrawHeader(oled, "HUMIDITY");

    snprintf(buf, sizeof(buf), "%.1f %%", data->humidity);
    OLED_DrawString(oled, 10, 20, buf, 2);

    uint8_t bar_progress = (uint8_t)(data->humidity);
    OLED_DrawProgressBar(oled, 10, 50, 108, 8, bar_progress);
}

static void DrawLightPage(OLED_t *oled, SensorData_t *data) {
    char buf[32];
    DrawHeader(oled, "LIGHT LEVEL");

    snprintf(buf, sizeof(buf), "%d", data->light_level);
    OLED_DrawString(oled, 10, 20, buf, 2);

    uint8_t bar_progress = (uint8_t)((uint32_t)data->light_level * 100 / 4095);
    OLED_DrawProgressBar(oled, 10, 50, 108, 8, bar_progress);
}

static void DrawMotionPage(OLED_t *oled, SensorData_t *data) {
    DrawHeader(oled, "MOTION");

    const char *motion_str = data->motion_detected ? "DETECTED" : "NONE";
    OLED_DrawString(oled, 10, 25, "State:", 1);
    OLED_DrawString(oled, 10, 40, motion_str, 2);
}

void DisplayTask(void *pvParameters) {
    DisplayTaskParams_t *params = (DisplayTaskParams_t *)pvParameters;
    SensorData_t data;
    DisplayPage_t current_page = PAGE_TEMPERATURE;

    OLED_Clear(params->oled);
    OLED_DrawString(params->oled, 10, 25, "Initializing...", 2);
    OLED_Update(params->oled);

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        xQueueReceive(params->display_page_queue, &current_page, 0);

        /* Consume event bits here: InputTask and MotionTask use the event
           group as an RTOS-level event channel, while queues carry payloads. */
        (void)xEventGroupClearBits(params->event_group,
                                   MOTION_DETECTED_BIT | ENCODER_CW_BIT |
                                   ENCODER_CCW_BIT | ENCODER_BTN_BIT);

        if (xQueueReceive(params->sensor_queue, &data, 0) == pdPASS) {
            SystemState_t state = StateMachine_GetState(params->state_machine);

            OLED_Clear(params->oled);

            if (state == STATE_ACTIVE) {
                DrawStatusBar(params->oled, state);

                switch (current_page) {
                    case PAGE_TEMPERATURE:
                        DrawTemperaturePage(params->oled, &data);
                        break;
                    case PAGE_HUMIDITY:
                        DrawHumidityPage(params->oled, &data);
                        break;
                    case PAGE_LIGHT:
                        DrawLightPage(params->oled, &data);
                        break;
                    case PAGE_MOTION:
                        DrawMotionPage(params->oled, &data);
                        break;
                    default:
                        break;
                }
            } else {
                OLED_DrawString(params->oled, 10, 25, "SYSTEM", 2);
                OLED_DrawString(params->oled, 10, 45, "INACTIVE", 2);
            }

            OLED_Update(params->oled);
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}
