#include "oled.h"
#include "font.h"

static HAL_StatusTypeDef OLED_SendCommand(OLED_t *oled, uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    /* Instrumentation: start from a clean ErrorCode.  HAL_BUSY and the bare
       HAL_TIMEOUT paths can return without touching it, so without this the
       latch below could capture a bit left over from an earlier transaction and
       mislabel the fault. */
    oled->hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1,
                                                       data, 2, OLED_I2C_TIMEOUT_MS);
    if (status != HAL_OK) {
        /* Instrumentation: latch this operation's first failure.  Both callers
           bail on the first failure, so at most one lands per operation -- and
           they clear the field before starting.  Latching unconditionally keeps
           this in step with the frame-transfer path below. */
        oled->last_error = oled->hi2c->ErrorCode;
    }
    return status;
}

HAL_StatusTypeDef OLED_Init(OLED_t *oled, I2C_HandleTypeDef *hi2c) {
    oled->hi2c = hi2c;
    oled->last_status = HAL_ERROR;
    oled->last_error = 0;
    HAL_Delay(100);

    static const uint8_t init_seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF,
    };

    HAL_StatusTypeDef status = HAL_OK;
    for (uint32_t i = 0; i < sizeof(init_seq); i++) {
        HAL_StatusTypeDef s = OLED_SendCommand(oled, init_seq[i]);
        if (s != HAL_OK) {
            /* Latch the first failure and stop: every later command would
               fail the same way, and the caller only needs to know that the
               panel never acknowledged. */
            status = s;
            break;
        }
    }

    OLED_Clear(oled);
    if (status == HAL_OK) {
        status = OLED_Update(oled);
    }
    oled->last_status = status;
    return status;
}

void OLED_Clear(OLED_t *oled) {
    for (int i = 0; i < sizeof(oled->buffer); i++) {
        oled->buffer[i] = 0;
    }
}

HAL_StatusTypeDef OLED_Update(OLED_t *oled) {
    /* Instrumentation: report this operation's first failure, not one left
       over from an earlier refresh. */
    oled->last_error = 0;

    /* Set column and page address window to full screen.  Bail out on the
       first failure so a dead panel costs one timeout, not six -- and keep the
       real HAL status instead of collapsing it to HAL_ERROR, so the caller can
       still tell a NACK from a timeout from a stuck bus. */
    static const uint8_t window_cmds[] = {
        0x21, 0, OLED_WIDTH - 1, 0x22, 0, (OLED_HEIGHT / 8) - 1,
    };
    HAL_StatusTypeDef status = HAL_OK;
    for (uint32_t i = 0; i < sizeof(window_cmds); i++) {
        HAL_StatusTypeDef s = OLED_SendCommand(oled, window_cmds[i]);
        if (s != HAL_OK) {
            status = s;
            break;
        }
    }

    if (status != HAL_OK) {
        oled->last_status = status;
        return status;
    }

    /* Send entire frame buffer in ONE I2C transaction:
     * [0x40][pixel0][pixel1]...[pixel1023]
     * This avoids 1024 separate I2C transactions which is far too slow
     * and causes Wokwi's I2C simulation to time out. */
    static uint8_t tx_buf[1 + OLED_WIDTH * OLED_HEIGHT / 8];
    tx_buf[0] = 0x40; /* Co=0, D/C#=1 — data stream */
    for (int i = 0; i < (int)sizeof(oled->buffer); i++) {
        tx_buf[1 + i] = oled->buffer[i];
    }
    /* Instrumentation: fresh ErrorCode, as in OLED_SendCommand. */
    oled->hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
    HAL_StatusTypeDef tx = HAL_I2C_Master_Transmit(oled->hi2c, OLED_I2C_ADDR << 1,
                                                   tx_buf, sizeof(tx_buf),
                                                   OLED_I2C_TIMEOUT_MS);
    if (tx != HAL_OK) {
        /* Instrumentation: keep the HAL reason behind the failure, and keep
           the real status rather than flattening it to HAL_ERROR. */
        oled->last_error = oled->hi2c->ErrorCode;
        status = tx;
    }

    oled->last_status = status;
    return status;
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

/* Instrumentation, not production behaviour: reduce the retained HAL status and
   ErrorCode to the one word that says which failure this was.  Status is tested
   before the ErrorCode bits because the F1 HAL can return HAL_BUSY without
   touching ErrorCode at all, leaving a stale AF bit from an earlier transaction
   that would otherwise be reported as a NACK. */
const char *OLED_FaultName(const OLED_t *oled) {
    if (oled->last_status == HAL_OK) {
        return "ok";
    }
    if (oled->last_status == HAL_BUSY) {
        return "busy";
    }
    if ((oled->last_error & HAL_I2C_ERROR_AF) != 0U) {
        return "NACK";
    }
    if ((oled->last_error & HAL_I2C_ERROR_TIMEOUT) != 0U) {
        return "timeout";
    }
    if (oled->last_status == HAL_TIMEOUT) {
        return "start";
    }
    return "error";
}
