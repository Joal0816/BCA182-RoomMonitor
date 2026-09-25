#include "oled.h"
#include "font.h"

static HAL_StatusTypeDef OLED_SendCommand(OLED_t *oled, uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    return HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1, data, 2, 100);
}

static HAL_StatusTypeDef OLED_SendData(OLED_t *oled, const uint8_t *data, uint16_t size) {
    uint8_t buffer[OLED_WIDTH + 1];

    if (size > OLED_WIDTH) {
        return HAL_ERROR;
    }

    buffer[0] = 0x40;
    for (uint16_t i = 0; i < size; i++) {
        buffer[i + 1] = data[i];
    }
    return HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1,
                                   buffer, size + 1, 100);
}

HAL_StatusTypeDef OLED_Init(OLED_t *oled, I2C_HandleTypeDef *hi2c) {
    oled->hi2c = hi2c;
    oled->ready = 0;
    HAL_Delay(100);

    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        if (HAL_I2C_IsDeviceReady(oled->hi2c, OLED_I2C_ADDR << 1, 1, 20) == HAL_OK) {
            oled->ready = 1;
            break;
        }
        HAL_Delay(10);
    }
    if (!oled->ready) {
        return HAL_ERROR;
    }

    static const uint8_t init_commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (uint8_t i = 0; i < sizeof(init_commands); i++) {
        if (OLED_SendCommand(oled, init_commands[i]) != HAL_OK) {
            oled->ready = 0;
            return HAL_ERROR;
        }
    }

    OLED_Clear(oled);
    if (OLED_Update(oled) != HAL_OK) {
        oled->ready = 0;
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_DisplayOn(OLED_t *oled) {
    if (oled->ready) {
        return OLED_SendCommand(oled, 0xAF);
    }
    return HAL_ERROR;
}

HAL_StatusTypeDef OLED_DisplayOff(OLED_t *oled) {
    if (oled->ready) {
        return OLED_SendCommand(oled, 0xAE);
    }
    return HAL_ERROR;
}

void OLED_Clear(OLED_t *oled) {
    for (int i = 0; i < sizeof(oled->buffer); i++) {
        oled->buffer[i] = 0;
    }
}

HAL_StatusTypeDef OLED_Update(OLED_t *oled) {
    if (!oled->ready) {
        return HAL_ERROR;
    }

    if (OLED_SendCommand(oled, 0x21) != HAL_OK ||
        OLED_SendCommand(oled, 0) != HAL_OK ||
        OLED_SendCommand(oled, OLED_WIDTH - 1) != HAL_OK ||
        OLED_SendCommand(oled, 0x22) != HAL_OK ||
        OLED_SendCommand(oled, 0) != HAL_OK ||
        OLED_SendCommand(oled, (OLED_HEIGHT / 8) - 1) != HAL_OK) {
        return HAL_ERROR;
    }

    for (uint8_t page = 0; page < OLED_HEIGHT / 8; page++) {
        if (OLED_SendData(oled, &oled->buffer[page * OLED_WIDTH], OLED_WIDTH) != HAL_OK) {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

void OLED_SetPixel(OLED_t *oled, uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    if (color) {
        oled->buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y & 7));
    } else {
        oled->buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y & 7));
    }
}

void OLED_DrawChar(OLED_t *oled, uint8_t x, uint8_t y, char c, uint8_t size) {
    if (c < 32 || c > 126) c = '?';

    for (int i = 0; i < 5 * size; i++) {
        for (int j = 0; j < 7 * size; j++) {
            uint8_t pixel = (font5x7[c - 32][i / size] >> (j / size)) & 1;
            OLED_SetPixel(oled, x + i, y + j, pixel);
        }
    }
}

void OLED_DrawString(OLED_t *oled, uint8_t x, uint8_t y, const char *str, uint8_t size) {
    while (*str) {
        if (x + 5 * size > OLED_WIDTH) {
            x = 0;
            y += 7 * size + 2;
        }
        if (y + 7 * size > OLED_HEIGHT) break;

        OLED_DrawChar(oled, x, y, *str, size);
        x += 5 * size + size;
        str++;
    }
}

void OLED_DrawLine(OLED_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t cx = x0;
    int16_t cy = y0;

    while (1) {
        if (cx >= 0 && cx < OLED_WIDTH && cy >= 0 && cy < OLED_HEIGHT) {
            OLED_SetPixel(oled, (uint8_t)cx, (uint8_t)cy, color);
        }
        if (cx == x1 && cy == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            cx += sx;
        }
        if (e2 < dx) {
            err += dx;
            cy += sy;
        }
    }
}

void OLED_DrawRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    OLED_DrawLine(oled, x, y, x + w - 1, y, color);
    OLED_DrawLine(oled, x + w - 1, y, x + w - 1, y + h - 1, color);
    OLED_DrawLine(oled, x + w - 1, y + h - 1, x, y + h - 1, color);
    OLED_DrawLine(oled, x, y + h - 1, x, y, color);
}

void OLED_FillRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint16_t i = x; i < (uint16_t)x + w && i < OLED_WIDTH; i++) {
        for (uint16_t j = y; j < (uint16_t)y + h && j < OLED_HEIGHT; j++) {
            OLED_SetPixel(oled, (uint8_t)i, (uint8_t)j, color);
        }
    }
}

void OLED_DrawProgressBar(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t progress) {
    OLED_DrawRect(oled, x, y, w, h, 1);
    uint8_t fill_w = (uint8_t)((uint32_t)w * progress / 100);
    if (fill_w > 0) {
        OLED_FillRect(oled, x + 1, y + 1, fill_w - 1, h - 2, 1);
    }
}
