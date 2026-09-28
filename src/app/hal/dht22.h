#ifndef DHT22_H
#define DHT22_H

#include "stm32f1xx_hal.h"

#define DHT22_OK       0
#define DHT22_ERROR    1
#define DHT22_TIMEOUT  2

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    float temperature;
    float humidity;
    uint8_t last_data[5];
} DHT22_t;

/* Initialises the driver and puts the data line into its idle state.
   Precondition: the GPIO *port* clock must already be enabled (MX_GPIO_Init
   does this during boot).  The driver changes pin direction by writing CRL/CRH
   directly instead of calling HAL_GPIO_Init, and a direct register write cannot
   turn a clock on for you. */
void DHT22_Init(DHT22_t *dht, GPIO_TypeDef *port, uint16_t pin);
uint8_t DHT22_Read(DHT22_t *dht);
float DHT22_GetTemperature(const DHT22_t *dht);
float DHT22_GetHumidity(const DHT22_t *dht);

#endif /* DHT22_H */
