#ifndef OLED_H
#define OLED_H

#include "stm32f1xx_hal.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDR  0x3C

/* Per-transaction I2C timeout.  This must be finite: with HAL_MAX_DELAY a
   panel that never acknowledges (absent, unpowered, or a stuck bus) blocks
   the caller forever, and since OLED_Init runs before the scheduler starts
   the whole application wedges with no diagnostic.  50 ms is ~50x the
   worst-case transfer time at 100 kHz, so a healthy panel never hits it. */
#define OLED_I2C_TIMEOUT_MS  50

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t buffer[OLED_WIDTH * OLED_HEIGHT / 8];
    /* Result of the most recent I2C transaction.  HAL_OK means the panel
       acknowledged; anything else means the panel is absent or the bus is
       stuck.  Without this the driver cannot tell "OLED ACKed" from
       "OLED not on the bus", because HAL_I2C_Master_Transmit's status was
       previously discarded. */
    HAL_StatusTypeDef last_status;
} OLED_t;

/* Returns HAL_OK only if the whole init sequence was acknowledged. */
HAL_StatusTypeDef OLED_Init(OLED_t *oled, I2C_HandleTypeDef *hi2c);
void OLED_Clear(OLED_t *oled);
/* Returns the status of the frame-buffer transfer. */
HAL_StatusTypeDef OLED_Update(OLED_t *oled);
void OLED_SetPixel(OLED_t *oled, uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(OLED_t *oled, uint8_t x, uint8_t y, char c, uint8_t size);
void OLED_DrawString(OLED_t *oled, uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_DrawLine(OLED_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_DrawRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_FillRect(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawProgressBar(OLED_t *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t progress);

#endif /* OLED_H */
