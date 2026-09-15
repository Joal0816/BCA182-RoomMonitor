#ifndef PIR_H
#define PIR_H

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    volatile uint8_t motion_detected;
    volatile uint32_t last_trigger_tick;
} PIR_t;

void PIR_Init(PIR_t *pir, GPIO_TypeDef *port, uint16_t pin);
uint8_t PIR_GetState(PIR_t *pir);
void PIR_EXTI_Callback(PIR_t *pir);

#endif /* PIR_H */
