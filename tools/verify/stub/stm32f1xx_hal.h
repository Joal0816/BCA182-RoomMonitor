#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H
#include <stdint.h>

typedef struct { uint32_t CTRL; uint32_t CYCCNT; } DWT_Type;
typedef struct { uint32_t PLLState; uint32_t PLLSource; uint32_t PLLMUL; } RCC_PLLInitTypeDef;
typedef struct { uint32_t DHCSR, DCRSR, DCRDR, DEMCR; } CoreDebug_Type;
extern DWT_Type *DWT;
#define CoreDebug ((CoreDebug_Type*)0xE000EDF0)
#define CoreDebug_DEMCR_TRCENA_Msk 0x01000000U
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define DISABLE 0U
#define ENABLE  1U

typedef struct { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t ClockType; uint32_t SYSCLKSource; uint32_t AHBCLKDivider; uint32_t APB1CLKDivider; uint32_t APB2CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t OscillatorType; uint32_t HSEState; uint32_t HSEPredivValue; uint32_t HSIState; uint32_t HSICalibrationValue; RCC_PLLInitTypeDef PLL; } RCC_OscInitTypeDef;
typedef struct { uint32_t Prescaler; uint32_t CounterMode; uint32_t Period; uint32_t ClockDivision; uint32_t AutoReloadPreload; } TIM_Base_InitTypeDef;
typedef struct { uint32_t OCMode; uint32_t Pulse; uint32_t OCPolarity; uint32_t OCFastMode; } TIM_OC_InitTypeDef;
typedef struct { uint32_t ClockSpeed; uint32_t DutyCycle; uint32_t OwnAddress1; uint32_t AddressingMode; uint32_t DualAddressMode; uint32_t OwnAddress2; uint32_t GeneralCallMode; uint32_t NoStretchMode; } I2C_InitTypeDef;
typedef struct { uint32_t DataAlign; uint32_t ScanConvMode; uint32_t ContinuousConvMode; uint32_t DiscontinuousConvMode; uint32_t ExternalTrigConv; uint32_t NbrOfConversion; } ADC_InitTypeDef;
typedef struct { uint32_t Channel; uint32_t Rank; uint32_t SamplingTime; } ADC_ChannelConfTypeDef;
typedef struct { uint32_t BaudRate; uint32_t WordLength; uint32_t StopBits; uint32_t Parity; uint32_t Mode; uint32_t HwFlowCtl; uint32_t OverSampling; } UART_InitTypeDef;

typedef struct {
    uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} GPIO_TypeDef;
typedef struct { void *Instance; I2C_InitTypeDef Init; uint32_t State; uint32_t ErrorCode; } I2C_HandleTypeDef;
typedef struct { void *Instance; ADC_InitTypeDef Init; } ADC_HandleTypeDef;
typedef struct { void *Instance; TIM_Base_InitTypeDef Init; } TIM_HandleTypeDef;
typedef struct { void *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
typedef enum { HAL_OK=0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PinState_RESET = 0, GPIO_PinState_SET = 1 } GPIO_PinState;

#define I2C1   ((void*)0x40005400)
#define TIM4   ((void*)0x40000800)
#define ADC1   ((void*)0x40012400)
#define USART1 ((void*)0x40013800)
#define GPIOA ((GPIO_TypeDef*)0x40010800)
#define GPIOB ((GPIO_TypeDef*)0x40010C00)
#define GPIOC ((GPIO_TypeDef*)0x40011000)
#define GPIOD ((GPIO_TypeDef*)0x40011400)
#define GPIOE ((GPIO_TypeDef*)0x40011800)

#define GPIO_PIN_0  ((uint16_t)0x0001)
#define GPIO_PIN_1  ((uint16_t)0x0002)
#define GPIO_PIN_2  ((uint16_t)0x0004)
#define GPIO_PIN_3  ((uint16_t)0x0008)
#define GPIO_PIN_4  ((uint16_t)0x0010)
#define GPIO_PIN_6  ((uint16_t)0x0040)
#define GPIO_PIN_7  ((uint16_t)0x0080)
#define GPIO_PIN_8  ((uint16_t)0x0100)
#define GPIO_PIN_9  ((uint16_t)0x0200)
#define GPIO_PIN_10 ((uint16_t)0x0400)
#define GPIO_PIN_13 ((uint16_t)0x2000)
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET   1
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_OUTPUT_OD 7U
#define GPIO_MODE_AF_PP 2U
#define GPIO_MODE_ANALOG 3U
#define GPIO_MODE_IT_RISING_FALLING 4U
#define GPIO_MODE_IT_FALLING 5U
#define GPIO_MODE_AF_OD 6U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define ADC_CHANNEL_0 0U
#define ADC_REGULAR_RANK_1 1U
#define ADC_SAMPLETIME_239CYCLES_5 7U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_DISABLE 0U
#define ADC_SOFTWARE_START 0U
#define TIM_CHANNEL_3 3U
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_OCMODE_PWM1 0U
#define TIM_OCPOLARITY_HIGH 0U
#define TIM_OCFAST_DISABLE 0U
#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 0U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U
#define RCC_OSCILLATORTYPE_HSE 1U
#define RCC_OSCILLATORTYPE_HSI 2U
#define RCC_HSE_ON 1U
#define RCC_HSE_PREDIV_DIV1 0U
#define RCC_HSI_ON 1U
#define RCC_HSICALIBRATION_DEFAULT 0x10U
#define RCC_PLL_ON 1U
#define RCC_PLL_NONE 0U
#define RCC_PLLSOURCE_HSE 1U
#define RCC_PLL_MUL9 9U
#define RCC_CLOCKTYPE_SYSCLK 1U
#define RCC_CLOCKTYPE_HCLK 2U
#define RCC_CLOCKTYPE_PCLK1 4U
#define RCC_CLOCKTYPE_PCLK2 8U
#define RCC_SYSCLKSOURCE_PLLCLK 1U
#define RCC_SYSCLKSOURCE_HSI 0U
#define RCC_SYSCLK_DIV1 0U
#define RCC_HCLK_DIV1 0U
#define RCC_HCLK_DIV2 1U
#define I2C_ADDRESSINGMODE_7BIT 0U
#define I2C_DUALADDRESS_DISABLE 0U
#define I2C_GENERALCALL_DISABLE 0U
#define I2C_NOSTRETCH_DISABLE 0U
#define I2C_DUTYCYCLE_2 0U
/* HAL I2C state (stm32f1xx_hal_i2c.h): the peripheral has not been initialized. */
#define HAL_I2C_STATE_RESET 0x00U
#define FLASH_LATENCY_0 0U
#define FLASH_LATENCY_2 2U
#define HAL_MAX_DELAY 0xFFFFFFFFU
/* HAL I2C ErrorCode bits (stm32f1xx_hal_i2c.h). */
#define HAL_I2C_ERROR_NONE    0x00000000U
#define HAL_I2C_ERROR_AF      0x00000004U
#define HAL_I2C_ERROR_TIMEOUT 0x00000020U

typedef enum { EXTI0_IRQn=6, EXTI1_IRQn=7, EXTI2_IRQn=8, EXTI3_IRQn=9, EXTI4_IRQn=10,
               EXTI9_5_IRQn=23, EXTI15_10_IRQn=40, WWDG_IRQn=0, PVD_IRQn=1, TAMPER_IRQn=2,
               RTC_IRQn=3, FLASH_IRQn=4, RCC_IRQn=5, ADC1_2_IRQn=18, TIM4_IRQn=30,
               USART1_IRQn=37 } IRQn_Type;

#define __HAL_RCC_GPIOA_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_I2C1_CLK_ENABLE()   do{}while(0)
#define __HAL_RCC_ADC1_CLK_ENABLE()   do{}while(0)
#define __HAL_RCC_TIM4_CLK_ENABLE()   do{}while(0)
#define __HAL_RCC_USART1_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_GPIOD_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_GPIOE_CLK_ENABLE() do{}while(0)
#define __HAL_RCC_AFIO_CLK_ENABLE()  do{}while(0)
#define __HAL_RCC_I2C1_FORCE_RESET()   do{}while(0)
#define __HAL_RCC_I2C1_RELEASE_RESET() do{}while(0)
#define __HAL_RCC_PWR_CLK_ENABLE()   do{}while(0)
#define __HAL_AFIO_REMAP_SWJ_NOJTAG() do{}while(0)
#define __HAL_TIM_SET_AUTORELOAD(h,v) do{(void)(h);(void)(v);}while(0)
#define __HAL_TIM_SET_COMPARE(h,c,v)  do{(void)(h);(void)(c);(void)(v);}while(0)
#define __HAL_TIM_GET_AUTORELOAD(h)   ((uint32_t)(h)->Init.Period)

void HAL_Init(void);
void HAL_IncTick(void);
void HAL_NVIC_SystemReset(void);
void SystemInit(void);
void SystemCoreClockUpdate(void);
void HAL_Delay(uint32_t ms);
void Error_Handler(void);
extern uint32_t SystemCoreClock;
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *cfg);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *cfg, uint32_t lat);
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *cfg);
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef *p, uint16_t pin);
void HAL_GPIO_EXTI_IRQHandler(uint16_t pin);
void HAL_NVIC_SetPriority(int irq, uint32_t pre, uint32_t sub);
void HAL_NVIC_EnableIRQ(int irq);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a, uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t a, uint32_t trials, uint32_t t);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h, ADC_ChannelConfTypeDef *cfg);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *h, TIM_OC_InitTypeDef *c, uint32_t ch);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *h);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t);
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetHCLKFreq(void);
#endif
