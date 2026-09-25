#ifndef DISPLAY_PAGE_H
#define DISPLAY_PAGE_H

#include <stdint.h>

/*
 * Rotary-encoder navigation logic (FR-06).
 * Kept free of HAL / FreeRTOS so it can be unit tested on the host.
 */

typedef enum {
    PAGE_TEMPERATURE = 0,
    PAGE_HUMIDITY,
    PAGE_LIGHT,
    PAGE_MOTION,
    PAGE_COUNT
} DisplayPage_t;

DisplayPage_t DisplayPage_Next(DisplayPage_t page);
DisplayPage_t DisplayPage_Previous(DisplayPage_t page);
const char* DisplayPage_Name(DisplayPage_t page);

#endif /* DISPLAY_PAGE_H */
