#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include "app/hal/dht22.h"
#include "app/hal/ldr.h"
#include "app/hal/pir.h"
#include "app/hal/oled.h"
#include "app/hal/encoder.h"
#include "app/hal/buzzer.h"

#include "app/logic/state_machine.h"
#include "app/logic/temperature.h"
#include "app/logic/alarm.h"

#include "app/tasks/input_task.h"
#include "app/tasks/motion_task.h"
#include "app/tasks/sensor_task.h"
#include "app/tasks/alarm_task.h"
#include "app/tasks/display_task.h"

#include "drivers/uart_mutex.h"

extern void xPortSysTickHandler(void);

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART1_UART_Init(void);

static DHT22_t dht22;
static LDR_t ldr;
static PIR_t pir;
static OLED_t oled;
static Encoder_t encoder;
static Buzzer_t buzzer;

static StateMachine_t state_machine;
static Alarm_t alarm;

static UART_Mutex_t uart_mutex;

static QueueHandle_t sensor_queue;
static QueueHandle_t display_page_queue;
static EventGroupHandle_t event_group;

static InputTaskParams_t input_task_params;
static MotionTaskParams_t motion_task_params;
static SensorTaskParams_t sensor_task_params;
static AlarmTaskParams_t alarm_task_params;
static DisplayTaskParams_t display_task_params;

static I2C_HandleTypeDef hi2c1;
static ADC_HandleTypeDef hadc1;
static TIM_HandleTypeDef htim4;
static UART_HandleTypeDef huart1;

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();
    MX_TIM4_Init();
    MX_USART1_UART_Init();

    DHT22_Init(&dht22, GPIOA, GPIO_PIN_1);
    LDR_Init(&ldr, &hadc1, ADC_CHANNEL_0);
    PIR_Init(&pir, GPIOB, GPIO_PIN_0);
    OLED_Init(&oled, &hi2c1);
    Encoder_Init(&encoder, GPIOA, GPIO_PIN_2, GPIOA, GPIO_PIN_3, GPIOA, GPIO_PIN_4);
    Buzzer_Init(&buzzer, &htim4, TIM_CHANNEL_3);

    StateMachine_Init(&state_machine, INACTIVE_TIMEOUT_MS);
    Alarm_Init(&alarm, &buzzer);
    UART_Mutex_Init(&uart_mutex, &huart1);

    sensor_queue = xQueueCreate(5, sizeof(SensorData_t));
    display_page_queue = xQueueCreate(1, sizeof(DisplayPage_t));
    event_group = xEventGroupCreate();

    input_task_params.encoder = &encoder;
    input_task_params.display_queue = display_page_queue;
    input_task_params.event_group = event_group;
    input_task_params.uart_mutex = &uart_mutex;

    motion_task_params.pir = &pir;
    motion_task_params.event_group = event_group;
    motion_task_params.sensor_queue = sensor_queue;
    motion_task_params.uart_mutex = &uart_mutex;

    sensor_task_params.dht22 = &dht22;
    sensor_task_params.ldr = &ldr;
    sensor_task_params.pir = &pir;
    sensor_task_params.sensor_queue = sensor_queue;
    sensor_task_params.uart_mutex = &uart_mutex;

    alarm_task_params.sensor_queue = sensor_queue;
    alarm_task_params.alarm = &alarm;
    alarm_task_params.uart_mutex = &uart_mutex;

    display_task_params.oled = &oled;
    display_task_params.sensor_queue = sensor_queue;
    display_task_params.display_page_queue = display_page_queue;
    display_task_params.state_machine = &state_machine;
    display_task_params.uart_mutex = &uart_mutex;

    xTaskCreate(InputTask, "InputTask", 256, &input_task_params, 3, &input_task_params.task_handle);
    xTaskCreate(MotionTask, "MotionTask", 256, &motion_task_params, 3, &motion_task_params.task_handle);
    xTaskCreate(SensorTask, "SensorTask", 512, &sensor_task_params, 2, NULL);
    xTaskCreate(AlarmTask, "AlarmTask", 256, &alarm_task_params, 2, NULL);
    xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 1, NULL);

    UART_Mutex_Printf(&uart_mutex, "[MAIN] System initialized\r\n");
    UART_Mutex_Printf(&uart_mutex, "[MAIN] Starting FreeRTOS scheduler\r\n");

    vTaskStartScheduler();

    while (1) {
    }
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_TIM4_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 0;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 65535;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (GPIO_Pin == GPIO_PIN_0) {
        PIR_EXTI_Callback(&pir);
        vTaskNotifyGiveFromISR(motion_task_params.task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else if (GPIO_Pin == GPIO_PIN_2) {
        Encoder_CLK_EXTI_Callback(&encoder);
        vTaskNotifyGiveFromISR(input_task_params.task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        encoder.button_pressed = 1;
        vTaskNotifyGiveFromISR(input_task_params.task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI2_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}

void EXTI4_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

void SysTick_Handler(void) {
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    while (1) {
    }
}

void vApplicationMallocFailedHook(void) {
    while (1) {
    }
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    UART_Mutex_Printf(&uart_mutex, "ASSERT: %s:%lu\r\n", file, line);
    Error_Handler();
}
#endif
