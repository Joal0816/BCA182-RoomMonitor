#include "dht22.h"

static void DHT22_SetOutput(DHT22_t *dht) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = dht->pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(dht->port, &GPIO_InitStruct);
}

static void DHT22_SetInput(DHT22_t *dht) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = dht->pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(dht->port, &GPIO_InitStruct);
}

static void DHT22_Delay_us(uint32_t us) {
    /* Use a simple volatile loop for microsecond-scale delays.
       DWT cycle counter is unreliable in simulation environments. */
    volatile uint32_t count = us * (SystemCoreClock / 1000000U / 10);
    while (count--) {
        __asm__ volatile("nop");
    }
}

static uint8_t DHT22_ComputeChecksum(uint8_t *data) {
    uint8_t crc = 0;
    for (int i = 0; i < 4; i++) {
        crc += data[i];
    }
    return crc;
}

void DHT22_Init(DHT22_t *dht, GPIO_TypeDef *port, uint16_t pin) {
    dht->port = port;
    dht->pin = pin;
    dht->temperature = 25.0f;
    dht->humidity = 50.0f;

    DHT22_SetOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
}

uint8_t DHT22_Read(DHT22_t *dht) {
    uint8_t data[5] = {0};
    uint8_t timeout;

    DHT22_SetOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_RESET);
    DHT22_Delay_us(18000);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
    DHT22_Delay_us(40);

    DHT22_SetInput(dht);

    timeout = 50;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            return DHT22_TIMEOUT;
        }
    }

    timeout = 50;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
        if (--timeout == 0) {
            return DHT22_TIMEOUT;
        }
    }

    timeout = 50;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            return DHT22_TIMEOUT;
        }
    }

    for (int i = 0; i < 40; i++) {
        timeout = 50;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
            if (--timeout == 0) {
                return DHT22_TIMEOUT;
            }
        }

        DHT22_Delay_us(30);

        timeout = 50;
        uint32_t pulse_count = 0;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
            pulse_count++;
            if (--timeout == 0) {
                break;
            }
        }

        data[i / 8] <<= 1;
        /* If pin stayed high for many loop iterations, it's a '1' bit (>28 µs).
           A '0' bit is only ~28 µs.  The loop counter captures this. */
        if (pulse_count > 15) {
            data[i / 8] |= 1;
        }
    }

    uint8_t crc = DHT22_ComputeChecksum(data);
    if (crc != data[4]) {
        return DHT22_ERROR;
    }

    for (int i = 0; i < 5; i++) {
        dht->last_data[i] = data[i];
    }

    dht->humidity = (float)((data[0] << 8) | data[1]) / 10.0f;
    dht->temperature = (float)(((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80) {
        dht->temperature = -dht->temperature;
    }

    return DHT22_OK;
}

float DHT22_GetTemperature(const DHT22_t *dht) {
    return dht->temperature;
}

float DHT22_GetHumidity(const DHT22_t *dht) {
    return dht->humidity;
}
