#ifndef OLED_H
#define OLED_H

#include "stm32f1xx_hal.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDR  0x3C

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t buffer[OLED_WIDTH * OLED_HEIGHT / 8];
} OLED_t;

void OLED_Init(OLED_t *oled, I2C_HandleTypeDef *hi2c);
void OLED_Clear(OLED_t *oled);
void OLED_Update(OLED_t *oled);
void OLED_SetPixel(OLED_t *oled, uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(OLED_t *oled, uint8_t x, uint8_t y, char c, uint8_t size);
void OLED_DrawString(OLED_t *oled, uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_DrawLine(OLED_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_DrawRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_FillRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawProgressBar(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t progress);

#endif /* OLED_H */
