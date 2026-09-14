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

void DHT22_Init(DHT22_t *dht, GPIO_TypeDef *port, uint16_t pin);
uint8_t DHT22_Read(DHT22_t *dht);
float DHT22_GetTemperature(DHT22_t *dht);
float DHT22_GetHumidity(DHT22_t *dht);
uint8_t DHT22_ComputeCRC(uint8_t *data);

#endif /* DHT22_H */
