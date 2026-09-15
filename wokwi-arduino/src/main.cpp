#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <event_groups.h>

#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─── Pin Definitions ────────────────────────────────────────────────────────
#define PIN_DHT22       PA1
#define PIN_LDR         A0
#define PIN_PIR         PB0
#define PIN_ENCODER_CLK PA2
#define PIN_ENCODER_DT  PA3
#define PIN_ENCODER_SW  PA4
#define PIN_BUZZER      PB8

// ─── Constants ──────────────────────────────────────────────────────────────
#define TEMP_LOW_THRESHOLD    18.0f
#define TEMP_HIGH_THRESHOLD   30.0f
#define INACTIVE_TIMEOUT_MS   15000U
#define SENSOR_READ_PERIOD_MS 1000U
#define ALARM_CHECK_PERIOD_MS 500U
#define DISPLAY_REFRESH_MS    100U

#define ALARM_FREQ_LOW     1000
#define ALARM_FREQ_HIGH    2000
#define ALARM_FREQ_PATTERN 1500

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define MOTION_DETECTED_BIT   (1 << 0)
#define ENCODER_CW_BIT        (1 << 1)
#define ENCODER_CCW_BIT       (1 << 2)
#define ENCODER_BTN_BIT       (1 << 3)

// ─── Types ──────────────────────────────────────────────────────────────────
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

typedef struct {
    SystemState_t current_state;
    uint32_t last_motion_tick;
    uint32_t timeout_ms;
} StateMachine_t;

typedef struct {
    uint8_t enabled;
    uint16_t current_freq;
    uint32_t last_toggle_tick;
    uint32_t toggle_interval_ms;
    uint8_t alarm_active;
} Alarm_t;

typedef struct {
    volatile int8_t delta;
    volatile uint8_t button_pressed;
    uint8_t last_clk;
} Encoder_t;

// ─── Globals ────────────────────────────────────────────────────────────────
static DHT dht(PIN_DHT22, DHT22);
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static QueueHandle_t sensor_queue;
static QueueHandle_t display_page_queue;
static EventGroupHandle_t event_group;
static SemaphoreHandle_t serial_mutex;

static StateMachine_t state_machine;
static Alarm_t alarm;
static Encoder_t encoder;

static volatile uint32_t g_tick_ms = 0;

// ─── SysTick ────────────────────────────────────────────────────────────────
extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
    g_tick_ms++;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

static uint32_t millis_ticks(void) {
    return g_tick_ms;
}

// ─── Temperature Logic ──────────────────────────────────────────────────────
static TempStatus_t EvaluateTemperature(float temperature) {
    if (temperature < TEMP_LOW_THRESHOLD) return TEMP_LOW;
    if (temperature > TEMP_HIGH_THRESHOLD) return TEMP_HIGH;
    return TEMP_NORMAL;
}

static const char* Temperature_GetStatusString(TempStatus_t status) {
    switch (status) {
        case TEMP_LOW:    return "LOW";
        case TEMP_NORMAL: return "NORMAL";
        case TEMP_HIGH:   return "HIGH";
        default:          return "UNKNOWN";
    }
}

static uint8_t Temperature_IsAlarm(TempStatus_t status) {
    return (status != TEMP_NORMAL);
}

// ─── State Machine ──────────────────────────────────────────────────────────
static void StateMachine_Init(StateMachine_t *sm, uint32_t timeout_ms) {
    sm->current_state = STATE_ACTIVE;
    sm->last_motion_tick = millis_ticks();
    sm->timeout_ms = timeout_ms;
}

static SystemState_t StateMachine_Update(StateMachine_t *sm, uint8_t motion_detected) {
    uint32_t now = millis_ticks();

    if (motion_detected) {
        sm->current_state = STATE_ACTIVE;
        sm->last_motion_tick = now;
        return sm->current_state;
    }

    if (sm->current_state == STATE_ACTIVE) {
        if ((now - sm->last_motion_tick) >= sm->timeout_ms) {
            sm->current_state = STATE_INACTIVE;
        }
    }

    return sm->current_state;
}

// ─── Buzzer ─────────────────────────────────────────────────────────────────
static void Buzzer_Play(uint16_t frequency) {
    tone(PIN_BUZZER, frequency);
}

static void Buzzer_Stop() {
    noTone(PIN_BUZZER);
}

// ─── Alarm ──────────────────────────────────────────────────────────────────
static void Alarm_Init(Alarm_t *a) {
    a->enabled = 1;
    a->current_freq = ALARM_FREQ_PATTERN;
    a->last_toggle_tick = 0;
    a->toggle_interval_ms = 500;
    a->alarm_active = 0;
}

static void Alarm_Update(Alarm_t *a, TempStatus_t temp_status) {
    if (!a->enabled) {
        if (a->alarm_active) {
            Buzzer_Stop();
            a->alarm_active = 0;
        }
        return;
    }

    if (Temperature_IsAlarm(temp_status)) {
        uint32_t now = millis_ticks();

        if (temp_status == TEMP_LOW)
            a->current_freq = ALARM_FREQ_LOW;
        else
            a->current_freq = ALARM_FREQ_HIGH;

        if (!a->alarm_active) {
            a->alarm_active = 1;
            a->last_toggle_tick = now;
            Buzzer_Play(a->current_freq);
        } else {
            if ((now - a->last_toggle_tick) >= a->toggle_interval_ms) {
                if (a->alarm_active) {
                    Buzzer_Stop();
                    a->alarm_active = 0;
                } else {
                    Buzzer_Play(a->current_freq);
                    a->alarm_active = 1;
                }
                a->last_toggle_tick = now;
            }
        }
    } else {
        if (a->alarm_active) {
            Buzzer_Stop();
            a->alarm_active = 0;
        }
    }
}

// ─── Serial Mutex Helper ────────────────────────────────────────────────────
static void MutexPrintf(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (xSemaphoreTake(serial_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.print(buffer);
        xSemaphoreGive(serial_mutex);
    }
}

// ─── Encoder ISR ────────────────────────────────────────────────────────────
static void encoder_clk_isr(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t clk = digitalRead(PIN_ENCODER_CLK);
    uint8_t dt  = digitalRead(PIN_ENCODER_DT);

    if (clk != encoder.last_clk) {
        encoder.last_clk = clk;
        if (clk == HIGH) {
            encoder.delta += (dt == LOW) ? 1 : -1;
        }
    }

    vTaskNotifyGiveFromISR(NULL, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void encoder_btn_isr(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    encoder.button_pressed = 1;
    vTaskNotifyGiveFromISR(NULL, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ═════════════════════════════════════════════════════════════════════════════
//  TASKS
// ═════════════════════════════════════════════════════════════════════════════

// ─── Sensor Task ────────────────────────────────────────────────────────────
static void SensorTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS);

    for (;;) {
        SensorData_t data;
        data.timestamp = millis_ticks();
        uint8_t read_ok = 1;

        float t = dht.readTemperature();
        float h = dht.readHumidity();

        if (isnan(t) || isnan(h)) {
            data.temperature = NAN;
            data.humidity = NAN;
            read_ok = 0;
            MutexPrintf("[SENSOR] DHT22 read error\r\n");
        } else {
            data.temperature = t;
            data.humidity = h;
        }

        int raw = analogRead(PIN_LDR);
        data.light_level = (uint16_t)raw;

        data.motion_detected = digitalRead(PIN_PIR);

        if (read_ok) {
            xQueueSend(sensor_queue, &data, 0);
        }

        MutexPrintf("[SENSOR] T=%.1fC H=%.1f%% L=%d M=%d\r\n",
                     data.temperature, data.humidity,
                     data.light_level, data.motion_detected);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ─── Alarm Task ─────────────────────────────────────────────────────────────
static void AlarmTask(void *pvParameters) {
    (void)pvParameters;
    SensorData_t data;

    for (;;) {
        if (xQueueReceive(sensor_queue, &data, pdMS_TO_TICKS(ALARM_CHECK_PERIOD_MS)) == pdPASS) {
            StateMachine_Update(&state_machine, data.motion_detected);

            if (!isnan(data.temperature)) {
                TempStatus_t status = EvaluateTemperature(data.temperature);
                Alarm_Update(&alarm, status);

                if (Temperature_IsAlarm(status)) {
                    MutexPrintf("[ALARM] Temp=%.1fC Status=%s\r\n",
                                 data.temperature,
                                 Temperature_GetStatusString(status));
                }
            }
        }
    }
}

// ─── Motion Task ────────────────────────────────────────────────────────────
static void MotionTask(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint8_t motion_state = digitalRead(PIN_PIR);

        if (motion_state) {
            xEventGroupSetBits(event_group, MOTION_DETECTED_BIT);
        } else {
            xEventGroupClearBits(event_group, MOTION_DETECTED_BIT);
        }

        MutexPrintf("[MOTION] State: %s\r\n",
                     motion_state ? "DETECTED" : "CLEAR");
    }
}

// ─── Input Task ─────────────────────────────────────────────────────────────
static void InputTask(void *pvParameters) {
    (void)pvParameters;
    DisplayPage_t current_page = PAGE_TEMPERATURE;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int8_t delta = 0;
        taskENTER_CRITICAL();
        delta = encoder.delta;
        encoder.delta = 0;
        taskEXIT_CRITICAL();

        if (delta > 0) {
            current_page = (DisplayPage_t)((current_page + 1) % PAGE_COUNT);
            xEventGroupSetBits(event_group, ENCODER_CW_BIT);
            MutexPrintf("[INPUT] Page CW -> %d\r\n", current_page);
        } else if (delta < 0) {
            current_page = (DisplayPage_t)((current_page + PAGE_COUNT - 1) % PAGE_COUNT);
            xEventGroupSetBits(event_group, ENCODER_CCW_BIT);
            MutexPrintf("[INPUT] Page CCW -> %d\r\n", current_page);
        }

        uint8_t btn = 0;
        taskENTER_CRITICAL();
        btn = encoder.button_pressed;
        encoder.button_pressed = 0;
        taskEXIT_CRITICAL();

        if (btn) {
            xEventGroupSetBits(event_group, ENCODER_BTN_BIT);
            MutexPrintf("[INPUT] Button pressed\r\n");
        }

        xQueueOverwrite(display_page_queue, &current_page);
    }
}

// ─── Display Task ───────────────────────────────────────────────────────────
static void DrawHeader(const char *title) {
    display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(4, 3);
    display.print(title);
    display.setTextColor(SSD1306_WHITE);
}

static void DrawStatusBar(SystemState_t state) {
    const char *state_str = (state == STATE_ACTIVE) ? "ACTIVE" : "INACTIVE";
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(SCREEN_WIDTH - 48, 3);
    display.print(state_str);
    display.setTextColor(SSD1306_WHITE);
}

static void DrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progress) {
    display.drawRect(x, y, w, h, SSD1306_WHITE);
    int16_t fill_w = (int16_t)((uint32_t)progress * (w - 2) / 100);
    if (fill_w > 0) {
        display.fillRect(x + 1, y + 1, fill_w, h - 2, SSD1306_WHITE);
    }
}

static void DrawTemperaturePage(SensorData_t *data) {
    DrawHeader("TEMPERATURE");

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f C", data->temperature);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print(buf);

    TempStatus_t status = EvaluateTemperature(data->temperature);
    display.setTextSize(1);
    display.setCursor(10, 45);
    display.print("Status: ");
    display.print(Temperature_GetStatusString(status));
}

static void DrawHumidityPage(SensorData_t *data) {
    DrawHeader("HUMIDITY");

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %%", data->humidity);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print(buf);

    uint8_t bar_progress = (uint8_t)(data->humidity);
    DrawProgressBar(10, 50, 108, 8, bar_progress);
}

static void DrawLightPage(SensorData_t *data) {
    DrawHeader("LIGHT LEVEL");

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", data->light_level);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print(buf);

    uint8_t bar_progress = (uint8_t)((uint32_t)data->light_level * 100 / 4095);
    DrawProgressBar(10, 50, 108, 8, bar_progress);
}

static void DrawMotionPage(SensorData_t *data) {
    DrawHeader("MOTION");

    display.setTextSize(1);
    display.setCursor(10, 25);
    display.print("State:");

    display.setTextSize(2);
    display.setCursor(10, 40);
    display.print(data->motion_detected ? "DETECTED" : "NONE");
}

static void DisplayTask(void *pvParameters) {
    (void)pvParameters;
    SensorData_t data;
    DisplayPage_t current_page = PAGE_TEMPERATURE;

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 25);
    display.print("Initializing...");
    display.display();

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        xQueueReceive(display_page_queue, &current_page, 0);

        if (xQueueReceive(sensor_queue, &data, 0) == pdPASS) {
            SystemState_t state = state_machine.current_state;

            display.clearDisplay();

            if (state == STATE_ACTIVE) {
                DrawStatusBar(state);

                switch (current_page) {
                    case PAGE_TEMPERATURE:
                        DrawTemperaturePage(&data);
                        break;
                    case PAGE_HUMIDITY:
                        DrawHumidityPage(&data);
                        break;
                    case PAGE_LIGHT:
                        DrawLightPage(&data);
                        break;
                    case PAGE_MOTION:
                        DrawMotionPage(&data);
                        break;
                    default:
                        break;
                }
            } else {
                display.setTextSize(2);
                display.setCursor(10, 25);
                display.print("SYSTEM");
                display.setCursor(10, 45);
                display.print("INACTIVE");
            }

            display.display();
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}

// ─── Setup & Loop ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(PIN_LDR, INPUT);
    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
    pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);

    dht.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[FATAL] SSD1306 init failed");
        for (;;);
    }
    display.clearDisplay();
    display.display();

    serial_mutex = xSemaphoreCreateMutex();
    sensor_queue = xQueueCreate(5, sizeof(SensorData_t));
    display_page_queue = xQueueCreate(1, sizeof(DisplayPage_t));
    event_group = xEventGroupCreate();

    StateMachine_Init(&state_machine, INACTIVE_TIMEOUT_MS);
    Alarm_Init(&alarm);

    encoder.delta = 0;
    encoder.button_pressed = 0;
    encoder.last_clk = digitalRead(PIN_ENCODER_CLK);

    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), encoder_clk_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_SW), encoder_btn_isr, FALLING);

    xTaskCreate(SensorTask,  "SensorTask",  512, NULL, 2, NULL);
    xTaskCreate(AlarmTask,   "AlarmTask",   256, NULL, 2, NULL);
    xTaskCreate(MotionTask,  "MotionTask",  256, NULL, 3, NULL);
    xTaskCreate(InputTask,   "InputTask",   256, NULL, 3, NULL);
    xTaskCreate(DisplayTask, "DisplayTask", 512, NULL, 1, NULL);

    MutexPrintf("[MAIN] System initialized\r\n");
    MutexPrintf("[MAIN] Starting FreeRTOS scheduler\r\n");

    vTaskStartScheduler();
}

void loop() {
    // Empty - FreeRTOS handles everything
}
