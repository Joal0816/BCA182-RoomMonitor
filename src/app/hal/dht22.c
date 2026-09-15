#include "dht22.h"
#include "FreeRTOS.h"
#include "task.h"

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
    /* Use DWT cycle counter if available, otherwise fall back to volatile loop */
    if (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) {
        uint32_t start = DWT->CYCCNT;
        uint32_t ticks = us * (SystemCoreClock / 1000000U);
        uint32_t timeout = ticks + 100000;
        while ((DWT->CYCCNT - start) < ticks) {
            if (--timeout == 0) break;
        }
    } else {
        /* Fallback: approximate delay using volatile loop */
        volatile uint32_t count = us * (SystemCoreClock / 1000000U / 4);
        while (count--) { __asm__ volatile("nop"); }
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
    dht->temperature = 0.0f;
    dht->humidity = 0.0f;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    DHT22_SetOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
    DHT22_Delay_us(2000000);
}

uint8_t DHT22_Read(DHT22_t *dht) {
    uint8_t data[5] = {0};
    uint8_t timeout;

    taskENTER_CRITICAL();

    DHT22_SetOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_RESET);
    DHT22_Delay_us(18000);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
    DHT22_Delay_us(40);

    DHT22_SetInput(dht);

    timeout = 100;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    timeout = 100;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    timeout = 100;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    for (int i = 0; i < 40; i++) {
        timeout = 100;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
            if (--timeout == 0) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
        }

        DHT22_Delay_us(30);

        timeout = 100;
        uint32_t start = DWT->CYCCNT;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
            if (--timeout == 0) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
        }
        uint32_t elapsed = DWT->CYCCNT - start;

        data[i / 8] <<= 1;
        if (elapsed > (30U * (SystemCoreClock / 1000000U))) {
            data[i / 8] |= 1;
        }
    }

    taskEXIT_CRITICAL();

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
