#include "display_task.h"
#include <stdio.h>
#include <string.h>

/*
 * DisplayTask (priority 1) - sole owner of SSD1306 rendering and I2C access.
 *
 * It blocks waiting for new sensor samples / page selections. While the
 * system is INACTIVE the panel is switched off (see Part IX of the lab) and
 * no rendering occurs; the state is read atomically from the event group.
 */

static void DrawHeader(OLED_t *oled, const char *title) {
    OLED_DrawRect(oled, 0, 0, OLED_WIDTH, 12, 1);
    OLED_DrawString(oled, 4, 3, title, 1);
}

static void DrawStatusBar(OLED_t *oled) {
    OLED_DrawString(oled, OLED_WIDTH - 48, 3, "ACTIVE", 1);
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
    SensorData_t latest;
    memset(&latest, 0, sizeof(latest));

    DisplayPage_t current_page = PAGE_TEMPERATURE;
    uint8_t have_data = 0;
    uint8_t blanked = 0;

    if (!params->oled->ready) {
        UART_Mutex_Printf(params->uart_mutex,
                          "[DISPLAY] OLED unavailable; retrying\r\n");
    }

    if (params->oled->ready) {
        OLED_Clear(params->oled);
        OLED_DrawString(params->oled, 10, 25, "Initializing...", 2);
        if (OLED_Update(params->oled) != HAL_OK) {
            params->oled->ready = 0;
            UART_Mutex_Printf(params->uart_mutex,
                              "[DISPLAY] OLED update failed; retrying\r\n");
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    for (;;) {
        if (!params->oled->ready) {
            vTaskDelay(pdMS_TO_TICKS(500));
            if (OLED_Init(params->oled, params->oled->hi2c) == HAL_OK) {
                blanked = 0;
                have_data = 0;
                UART_Mutex_Printf(params->uart_mutex,
                                  "[DISPLAY] OLED recovered\r\n");
            }
            continue;
        }

        xQueueReceive(params->display_page_queue, &current_page, 0);

        if (xQueueReceive(params->display_queue, &data, 0) == pdPASS) {
            latest = data;
            have_data = 1;
        }

        EventBits_t bits = xEventGroupGetBits(params->event_group);
        uint8_t active = (bits & EVENT_STATE_ACTIVE_BIT) != 0;

        if (!active) {
            if (!blanked) {
                OLED_Clear(params->oled);
                if (OLED_Update(params->oled) != HAL_OK ||
                    OLED_DisplayOff(params->oled) != HAL_OK) {
                    params->oled->ready = 0;
                    UART_Mutex_Printf(params->uart_mutex,
                                      "[DISPLAY] OLED communication failed; retrying\r\n");
                    continue;
                }
                blanked = 1;
                UART_Mutex_Printf(params->uart_mutex, "[DISPLAY] OLED blanked (INACTIVE)\r\n");
            }
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
            continue;
        }

        if (blanked) {
            if (OLED_DisplayOn(params->oled) != HAL_OK) {
                params->oled->ready = 0;
                UART_Mutex_Printf(params->uart_mutex,
                                  "[DISPLAY] OLED communication failed; retrying\r\n");
                continue;
            }
            blanked = 0;
            UART_Mutex_Printf(params->uart_mutex, "[DISPLAY] OLED restored (ACTIVE)\r\n");
        }

        if (have_data) {
            OLED_Clear(params->oled);
            DrawStatusBar(params->oled);

            switch (current_page) {
                case PAGE_TEMPERATURE:
                    DrawTemperaturePage(params->oled, &latest);
                    break;
                case PAGE_HUMIDITY:
                    DrawHumidityPage(params->oled, &latest);
                    break;
                case PAGE_LIGHT:
                    DrawLightPage(params->oled, &latest);
                    break;
                case PAGE_MOTION:
                    DrawMotionPage(params->oled, &latest);
                    break;
                default:
                    break;
            }

            if (OLED_Update(params->oled) != HAL_OK) {
                params->oled->ready = 0;
                UART_Mutex_Printf(params->uart_mutex,
                                  "[DISPLAY] OLED update failed; retrying\r\n");
                continue;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}
