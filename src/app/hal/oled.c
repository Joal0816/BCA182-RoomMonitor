#include "oled.h"
#include "font.h"

static void OLED_SendCommand(OLED_t *oled, uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1, data, 2, 10);
}

static void OLED_SendData(OLED_t *oled, uint8_t data) {
    uint8_t buf[2] = {0x40, data};
    HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1, buf, 2, 10);
}

void OLED_Init(OLED_t *oled, I2C_HandleTypeDef *hi2c) {
    oled->hi2c = hi2c;
    HAL_Delay(100);

    OLED_SendCommand(oled, 0xAE);
    OLED_SendCommand(oled, 0xD5);
    OLED_SendCommand(oled, 0x80);
    OLED_SendCommand(oled, 0xA8);
    OLED_SendCommand(oled, 0x3F);
    OLED_SendCommand(oled, 0xD3);
    OLED_SendCommand(oled, 0x00);
    OLED_SendCommand(oled, 0x40);
    OLED_SendCommand(oled, 0x8D);
    OLED_SendCommand(oled, 0x14);
    OLED_SendCommand(oled, 0x20);
    OLED_SendCommand(oled, 0x00);
    OLED_SendCommand(oled, 0xA1);
    OLED_SendCommand(oled, 0xC8);
    OLED_SendCommand(oled, 0xDA);
    OLED_SendCommand(oled, 0x12);
    OLED_SendCommand(oled, 0x81);
    OLED_SendCommand(oled, 0xCF);
    OLED_SendCommand(oled, 0xD9);
    OLED_SendCommand(oled, 0xF1);
    OLED_SendCommand(oled, 0xDB);
    OLED_SendCommand(oled, 0x40);
    OLED_SendCommand(oled, 0xA4);
    OLED_SendCommand(oled, 0xA6);
    OLED_SendCommand(oled, 0xAF);

    OLED_Clear(oled);
    OLED_Update(oled);
}

void OLED_Clear(OLED_t *oled) {
    for (int i = 0; i < sizeof(oled->buffer); i++) {
        oled->buffer[i] = 0;
    }
}

void OLED_Update(OLED_t *oled) {
    OLED_SendCommand(oled, 0x21);
    OLED_SendCommand(oled, 0);
    OLED_SendCommand(oled, OLED_WIDTH - 1);
    OLED_SendCommand(oled, 0x22);
    OLED_SendCommand(oled, 0);
    OLED_SendCommand(oled, (OLED_HEIGHT / 8) - 1);

    for (int i = 0; i < sizeof(oled->buffer); i++) {
        OLED_SendData(oled, oled->buffer[i]);
    }
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
