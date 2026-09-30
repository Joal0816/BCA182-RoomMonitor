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
#include "drivers/diag.h"

/* Point VTOR at the vector table at 0x08000000.
 *
 * This cannot be done by defining a function named SystemInit(): the startup
 * file's `bl SystemInit` resolves against the STM32Cube framework's
 * system_stm32f1xx.c, whose SystemInit is a plain strong definition (not
 * __attribute__((weak))), so a same-named definition here is a duplicate
 * symbol at link time.  The framework's copy compiles to a bare `bx lr`, so
 * nothing in the boot path has ever programmed VTOR and it keeps its reset
 * value of 0.  That is invisible on any part whose flash is aliased to
 * address 0 -- which is exactly what tieing BOOT0 low selects on a real Blue
 * Pill -- but where the alias is absent the core reads the vector table from
 * unmapped memory, which on a Cortex-M3 presents as a hard fault taken before
 * the first instruction of main() runs.  Symptom: a dead board with no UART
 * output and a blank OLED.
 *
 * Calling it from main() instead of from Reset_Handler is safe: the only
 * consumer of VTOR is the FreeRTOS Cortex-M port, which validates the SVCall
 * and PendSV slots in xPortStartScheduler() -- long after main() has started.
 * The relocation therefore still precedes every read of the vector table. */
static void App_RelocateVectors(void) {
    Diag_RelocateVectors();
}

/* Declared here rather than reached through portYIELD_FROM_ISR().  This file
 * does not resolve include/portmacro.h: the FreeRTOS library puts its own
 * portable/GCC/ARM_CM3 directory ahead of every -I path from build_flags, so
 * src/main.c compiles against the stock header even though the patched port is
 * the one that gets linked.  The stock portYIELD_FROM_ISR would therefore store
 * PendSV-set to ICSR, and the patched port deliberately installs no PendSV
 * handler -- the slot stays a weak alias of Default_Handler, which is an
 * infinite loop, so the first PIR or encoder edge would wedge the MCU.  Calling
 * vPortYieldFromISR() directly sidesteps header resolution entirely. */
extern void vPortYieldFromISR(void);

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART1_UART_Init(void);
static void I2C1_BusProbeAndRecover(void);

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
static QueueHandle_t display_sensor_queue;
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
    App_RelocateVectors();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM4_Init();
    MX_USART1_UART_Init();

    /* Latch the diagnostic register addresses before anything can fail, then
       report where the vector table actually lives.  The patched Cortex-M port
       in lib/freertos_port_patch owns the SVC and SysTick slots directly and
       does not install a PendSV handler, so FreeRTOS's handler-installation
       check is disabled (configCHECK_HANDLER_INSTALLATION == 0).  This line
       keeps the vector table visible in the log regardless. */
    Diag_Init();
    Diag_DumpVectorInfo();

    /* The UART mutex must exist before anything can report a fault.  OLED_Init
       below reports failures through it, so creating it afterwards leaves the
       failure path calling xSemaphoreTake(NULL), which trips a FreeRTOS
       configASSERT and hides the real message. */
    UART_Mutex_Init(&uart_mutex, &huart1);

    DHT22_Init(&dht22, GPIOA, GPIO_PIN_1);
    LDR_Init(&ldr, &hadc1, ADC_CHANNEL_0);
    PIR_Init(&pir, GPIOB, GPIO_PIN_0);
    /* Probe the bus and recover it if a slave is holding a line low, then
       bring I2C1 up from a clean state.  The init is deferred to here rather
       than the top of main() because the probe reports through the UART mutex,
       which does not exist yet at that point. */
    I2C1_BusProbeAndRecover();
    MX_I2C1_Init();

    /* --- Instrumentation: classify the OLED I2C failure (removable) --------
       Every SSD1306 transaction fails on the current board, but the existing
       message prints for any non-HAL_OK result, so a NACK is indistinguishable
       from a timeout or a stuck bus.  Print the retained HAL status and
       ErrorCode decoded to a word, then probe the address once with
       HAL_I2C_IsDeviceReady() for a binary ACK/NAK answer; the line levels the
       probe above already reported bracket the init.  This block exists only
       to classify the fault and can be removed once it is understood. */
    /* Report the I2C result: a panel that never ACKs is otherwise
       indistinguishable from a panel that is present but not being drawn to. */
    HAL_StatusTypeDef oled_status = OLED_Init(&oled, &hi2c1);
    if (oled_status != HAL_OK) {
        UART_Mutex_Printf(&uart_mutex, "[OLED] init failed: no ACK from 0x%02X\r\n",
                          OLED_I2C_ADDR);
    }

    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] bus after init: SCL(PB6)=%u SDA(PB7)=%u\r\n",
                      (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6),
                      (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7));
    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] init status=%d ErrorCode=0x%02X (%s)\r\n",
                      (int)oled_status, (unsigned)oled.last_error,
                      OLED_FaultName(&oled));
    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] probe 0x%02X: %s\r\n",
                      OLED_I2C_ADDR,
                      (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(OLED_I2C_ADDR << 1), 1U, 50U) == HAL_OK)
                          ? "address ACKs" : "address does not ACK");
    Encoder_Init(&encoder, GPIOA, GPIO_PIN_2, GPIOA, GPIO_PIN_3, GPIOA, GPIO_PIN_4);
    Buzzer_Init(&buzzer, &htim4, TIM_CHANNEL_3);

    StateMachine_Init(&state_machine, INACTIVE_TIMEOUT_MS);
    Alarm_Init(&alarm, &buzzer);

    sensor_queue = xQueueCreate(1, sizeof(SensorData_t));
    display_sensor_queue = xQueueCreate(1, sizeof(SensorData_t));
    display_page_queue = xQueueCreate(1, sizeof(DisplayPage_t));
    event_group = xEventGroupCreate();

    input_task_params.encoder = &encoder;
    input_task_params.display_queue = display_page_queue;
    input_task_params.event_group = event_group;
    input_task_params.uart_mutex = &uart_mutex;

    motion_task_params.pir = &pir;
    motion_task_params.event_group = event_group;
    motion_task_params.uart_mutex = &uart_mutex;

    sensor_task_params.dht22 = &dht22;
    sensor_task_params.ldr = &ldr;
    sensor_task_params.pir = &pir;
    sensor_task_params.alarm_queue = sensor_queue;
    sensor_task_params.display_queue = display_sensor_queue;
    sensor_task_params.uart_mutex = &uart_mutex;

    alarm_task_params.sensor_queue = sensor_queue;
    alarm_task_params.alarm = &alarm;
    alarm_task_params.state_machine = &state_machine;
    alarm_task_params.uart_mutex = &uart_mutex;

    display_task_params.oled = &oled;
    display_task_params.sensor_queue = display_sensor_queue;
    display_task_params.display_page_queue = display_page_queue;
    display_task_params.state_machine = &state_machine;
    display_task_params.event_group = event_group;
    display_task_params.uart_mutex = &uart_mutex;

    BaseType_t created;

    /* Every xTaskCreate return code used to be discarded.  Each task needs
       its TCB and stack carved out of the 12 KB FreeRTOS heap, so a failure
       here means the task silently never runs -- indistinguishable, from the
       outside, from the board not booting at all.  Report them. */
    created  = xTaskCreate(InputTask, "InputTask", 256, &input_task_params, 3, &input_task_params.task_handle);
    created |= xTaskCreate(MotionTask, "MotionTask", 256, &motion_task_params, 3, &motion_task_params.task_handle);
    created |= xTaskCreate(SensorTask, "SensorTask", 512, &sensor_task_params, 2, NULL);
    /* AlarmTask formats "%.1f" through UART_Mutex_Printf.  That call pulls the
       newlib float formatter (_svfprintf_r -> _printf_float -> _dtoa_r) into
       this task and, because UART_Mutex_Printf's 256-byte formatting buffer is
       a local in that function rather than a static, spends that much again on
       this task's stack for the duration of the call.  256
       words (1 KB) did not cover both together, which is why the task tripped
       configCHECK_FOR_STACK_OVERFLOW once DHT22 reads started succeeding and
       the alarm line became reachable.  512 words matches SensorTask and
       DisplayTask, the two tasks that already format floats safely. */
    created |= xTaskCreate(AlarmTask, "AlarmTask", 512, &alarm_task_params, 2, NULL);
    created |= xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 1, NULL);

    if (created != pdPASS) {
        UART_Mutex_Printf(&uart_mutex, "[MAIN] FATAL: xTaskCreate failed\r\n");
    }

    /* Instrumentation: the five stacks and their TCBs come out of a fixed
       12,288-byte heap, so raising AlarmTask's stack must not have quietly
       starved the others.  Report what is left once every allocation above
       has been made.  This figure is not steady-state headroom: the
       vTaskStartScheduler() below still has to allocate the idle task
       (configMINIMAL_STACK_SIZE, 128 words) and the timer daemon
       (configTIMER_TASK_STACK_DEPTH, 256 words) plus their TCBs. */
    UART_Mutex_Printf(&uart_mutex,
                      "[MAIN] FreeRTOS heap free after task creation: %u bytes\r\n",
                      (unsigned)xPortGetFreeHeapSize());

    UART_Mutex_Printf(&uart_mutex, "[MAIN] System initialized\r\n");
    UART_Mutex_Printf(&uart_mutex, "[MAIN] Starting FreeRTOS scheduler\r\n");

    vTaskStartScheduler();

    /* Reached only if the scheduler could not start: vTaskStartScheduler()
       returns to its caller when the idle task or the timer task could not be
       created.  Previously this loop was silent, so the one failure mode that
       explains "main's banner printed but no task ever ran" looked exactly
       like a healthy board. */
    Diag_Puts("[MAIN] FATAL: vTaskStartScheduler returned - out of heap?\r\n");

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
        /* No HSE crystal, so PLL x9 to 72 MHz is impossible.  Fall back to the
           internal 8 MHz RC oscillator rather than halting: every clock in this
           design (USART1 baud, I2C1 timing, TIM4 prescaler, the FreeRTOS tick)
           is derived from SystemCoreClock, so firmware clocked at 8 MHz runs
           correctly, just slower.  This is what makes the project survive a
           board or simulator without a crystal. */
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            Error_Handler();
        }

        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                    | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

        /* 8 MHz needs no wait states. */
        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
            Error_Handler();
        }
        return;
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

/* Probe the I2C1 bus and recover it if a stuck slave is holding a line low.
 *
 * Runs before MX_I2C1_Init(), while PB6/PB7 are still plain GPIO, so the pull
 * configuration is actually honoured: the STM32F1 HAL ignores the Pull field
 * for GPIO_MODE_AF_OD (see ST's stm32f1xx_hal_gpio.c), so an AF-mode read is
 * not a trustworthy view of the pads.  An input with a pull-up is.  SCL is
 * PB6, SDA is PB7.
 *
 * If a slave missed a STOP and is still driving SDA low, the bus is dead for
 * every later transfer.  The standard recovery is to clock SCL nine times to
 * walk that slave through the rest of its byte -- after which it must release
 * SDA -- and then issue a STOP.  The known-good Wokwi reference for this board
 * and pinout does the same before bringing the peripheral up.
 *
 * The pins are deliberately left as GPIO, in whatever mode this function last
 * set them: HAL_I2C_MspInit() reconfigures them as AF_OD from inside
 * HAL_I2C_Init(), so there is nothing to restore here. */
static void I2C1_BusProbeAndRecover(void) {
    GPIO_InitTypeDef probe = {0};
    unsigned scl;
    unsigned sda;
    int i;

    /* GPIOB and AFIO must be clocked before anything touches the pins; the
       HAL clock macros are idempotent, so repeating them is harmless. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* Input with pull-up: the only mode on the F1 where the pull setting is
       applied, and therefore the reliable way to read the actual line levels. */
    probe.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    probe.Mode = GPIO_MODE_INPUT;
    probe.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &probe);

    scl = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    sda = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] bus before init: SCL(PB6)=%u SDA(PB7)=%u\r\n",
                      scl, sda);

    if ((scl != 0U) && (sda != 0U)) {
        UART_Mutex_Printf(&uart_mutex, "[OLED] bus idle, recovery skipped\r\n");
        return;
    }

    /* Open-drain outputs: the external 4.7k pull-ups set the high level, and a
       low is only ever asserted by pulling the line down, so this cannot fight
       a slave that is still driving. */
    probe.Mode = GPIO_MODE_OUTPUT_OD;
    probe.Pull = GPIO_NOPULL;
    probe.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &probe);

    /* Release both lines and let the pull-ups settle them high.  SCL's output
       latch is still 0 from reset, so it must be written high explicitly or
       the line stays actively driven low for the whole settle delay. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* Nine SCL pulses with SDA free: enough to finish any partial byte the
       slave is holding, after which it must let SDA go. */
    for (i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_Delay(1);
    }

    /* A START followed by a STOP: SDA driven low while SCL is high (START),
       then released high while SCL stays high (STOP).  Together that is the
       NXP AN10216 bus-clear sequence.  HAL_Delay() works here: the FreeRTOS
       port advances the HAL tick from its tick hook even before the scheduler
       starts. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* Read back in the same mode as the opening probe.  An open-drain output
       with the latch high is released, but configured with no pull-up here it
       would only reflect whatever else holds the line, so the two reads would
       not be comparable. */
    probe.Mode = GPIO_MODE_INPUT;
    probe.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &probe);

    scl = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    sda = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] bus after recovery: SCL(PB6)=%u SDA(PB7)=%u\r\n",
                      scl, sda);

    /* Verdict, so a still-dead bus is not distinguishable only by reading two
       raw levels: the later failure-classification output can be read against
       this line.  A healthy bus returns here; the pins stay open-drain with
       both lines released, and HAL_I2C_MspInit() makes them AF_OD later. */
    if ((scl != 0U) && (sda != 0U)) {
        UART_Mutex_Printf(&uart_mutex, "[OLED] bus recovery succeeded\r\n");
        return;
    }
    UART_Mutex_Printf(&uart_mutex, "[OLED] BUS FAULT: lines still low after recovery\r\n");

    /* Instrumentation, still-low path only.  The input reads above prove a line
       is low, not why, so drive both pads push-pull high and read them back.
       A 1 means the pad drove the line high -- nothing was holding it low at
       that instant.  A 0 means the line stays low even against a full push-pull
       drive: either something external holds it or the pad itself is stuck, and
       this test cannot tell those apart.  Note that the opening and
       post-recovery reads were taken with the internal pull-up enabled, so a 0
       there already means something is driving the line or the model does not
       implement the pull-ups; a missing external pull-up alone would not do it.
       Remove once the fault is understood.

       Push-pull against a line a slave is still holding is contention; 1 ms is
       survivable here and harmless in Wokwi, but on real hardware shorten the
       assertion or fit a series resistor. */
    probe.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &probe);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);
    scl = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    sda = (unsigned)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    UART_Mutex_Printf(&uart_mutex,
                      "[OLED] drive test: SCL(PB6)=%u SDA(PB7)=%u\r\n",
                      scl, sda);

    /* Release the pads again before returning.  If HAL_I2C_Init() then fails and
       Error_Handler() traps, push-pull outputs left driving into a held line
       would source current indefinitely.  HAL_I2C_MspInit() makes them AF_OD
       when the peripheral is brought up. */
    probe.Mode = GPIO_MODE_INPUT;
    probe.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &probe);
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
    /* Mirror the known-good STM32duino bring-up sequence: its i2c_init() clocks
       the peripheral before forcing and releasing its reset, then marks the
       handle reset before HAL_I2C_Init(), so the peripheral starts from a clean
       state rather than whatever a previous owner (or the simulator) left
       behind.  __HAL_RCC_I2C1_CLK_ENABLE() repeats the one in HAL_I2C_MspInit()
       and is idempotent. */
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();
    /* The probe above deliberately leaves PB6/PB7 as plain GPIO; they become
       AF_OD again only because this State forces HAL_I2C_Init() to re-run
       HAL_I2C_MspInit().  A future re-init path that reaches HAL_I2C_Init()
       with State == READY would skip MspInit, leave the bus as GPIO, and
       silently kill every transfer. */
    hi2c1.State = HAL_I2C_STATE_RESET;
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

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        /* The F1 HAL ignores Pull for GPIO_MODE_AF_OD; the external 4.7 kOhm
           pull-ups are what hold the bus high. */
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (GPIO_Pin == GPIO_PIN_0) {
        PIR_EXTI_Callback(&pir);
        vTaskNotifyGiveFromISR(motion_task_params.task_handle, &xHigherPriorityTaskWoken);
    } else if (GPIO_Pin == GPIO_PIN_2) {
        Encoder_CLK_EXTI_Callback(&encoder);
        vTaskNotifyGiveFromISR(input_task_params.task_handle, &xHigherPriorityTaskWoken);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        encoder.button_pressed = 1;
        vTaskNotifyGiveFromISR(input_task_params.task_handle, &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken != pdFALSE) {
        vPortYieldFromISR();
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

/* HAL's time base, driven by the FreeRTOS tick.
 *
 * There is deliberately no SysTick_Handler() in the application: the patched
 * port in lib/freertos_port_patch/src/port.c owns the SysTick vector, because
 * it has to gate the tick interrupt inside critical sections.  The port calls
 * the application tick hook whenever the HAL time base needs advancing -- both
 * before the scheduler starts and on every subsequent tick -- so HAL_GetTick()
 * and HAL_Delay() keep working exactly as before. */
void vApplicationTickHook(void) {
    HAL_IncTick();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    UART_Mutex_Printf(&uart_mutex, "[FATAL] Stack overflow in task: %s\r\n", pcTaskName);
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 1000000; i++);
    }
}

void vApplicationMallocFailedHook(void) {
    UART_Mutex_Printf(&uart_mutex, "[FATAL] Malloc failed - heap exhausted\r\n");
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 500000; i++);
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
