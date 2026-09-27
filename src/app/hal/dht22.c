#include "dht22.h"
#include "FreeRTOS.h"
#include "task.h"

/* The longest level the DHT22 holds during a frame is 80 us.  Polling a GPIO
   through HAL at 72 MHz costs roughly 6-8 cycles per iteration, so 80 us spans
   several hundred iterations; the previous bound of 100 expired mid-frame and
   reported a timeout on every single read. */
#define DHT22_EDGE_TIMEOUT 2000U

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

/* DWT_CTRL is a writable configuration register: it reads back set as soon as
   software writes it, even on cores where CYCCNT never actually advances.
   Treating that bit as proof the counter runs routes every timed loop into a
   full-length timeout spin, so probe the counter once and cache the verdict. */
static uint8_t DHT22_CycleCounterReady(void) {
    static uint8_t ready = 2; /* 2 = unprobed, 0 = unusable, 1 = usable */

    if (ready == 2) {
        ready = 0;
        if (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) {
            uint32_t start = DWT->CYCCNT;
            for (volatile uint32_t i = 0; i < 64U; i++) {
                if (DWT->CYCCNT != start) {
                    ready = 1;
                    break;
                }
            }
        }
    }

    return ready;
}

static void DHT22_Delay_us(uint32_t us) {
    if (us == 0U) {
        return;
    }

    uint32_t cycles = us * (SystemCoreClock / 1000000U);

    if (DHT22_CycleCounterReady()) {
        uint32_t start = DWT->CYCCNT;
        uint32_t timeout = cycles + (SystemCoreClock / 100U);
        while ((DWT->CYCCNT - start) < cycles) {
            if (--timeout == 0) break;
        }
    } else {
        /* Fallback: approximate delay using volatile loop, calibrated at
           roughly four cycles per iteration. */
        uint32_t count = cycles / 4U;
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

    /* The sensor needs about 1 s to stabilise after power-up, but blocking
       here would stall the whole boot before the scheduler even starts.
       SensorTask absorbs that settle time with an RTOS delay instead. */
}

uint8_t DHT22_Read(DHT22_t *dht) {
    uint8_t data[5] = {0};
    uint32_t timeout;

    taskENTER_CRITICAL();

    DHT22_SetOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_RESET);
    DHT22_Delay_us(18000);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
    DHT22_Delay_us(40);

    DHT22_SetInput(dht);

    timeout = DHT22_EDGE_TIMEOUT;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    timeout = DHT22_EDGE_TIMEOUT;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    timeout = DHT22_EDGE_TIMEOUT;
    while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
        if (--timeout == 0) {
            taskEXIT_CRITICAL();
            return DHT22_TIMEOUT;
        }
    }

    for (int i = 0; i < 40; i++) {
        timeout = DHT22_EDGE_TIMEOUT;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_RESET) {
            if (--timeout == 0) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
        }

        /* Every bit starts with a 50 us low pulse.  A "0" then stays high for
           26-28 us and a "1" for 70 us, so sampling once past the 30 us mark
           discriminates the two without needing a cycle counter. */
        DHT22_Delay_us(30);
        data[i / 8] <<= 1;
        if (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
            data[i / 8] |= 1;
        }

        timeout = DHT22_EDGE_TIMEOUT;
        while (HAL_GPIO_ReadPin(dht->port, dht->pin) == GPIO_PIN_SET) {
            if (--timeout == 0) {
                taskEXIT_CRITICAL();
                return DHT22_TIMEOUT;
            }
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
