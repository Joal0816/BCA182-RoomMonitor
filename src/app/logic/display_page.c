#include "display_page.h"

DisplayPage_t DisplayPage_Next(DisplayPage_t page) {
    return (DisplayPage_t)((page + 1) % PAGE_COUNT);
}

DisplayPage_t DisplayPage_Previous(DisplayPage_t page) {
    return (DisplayPage_t)((page + PAGE_COUNT - 1) % PAGE_COUNT);
}

const char* DisplayPage_Name(DisplayPage_t page) {
    switch (page) {
        case PAGE_TEMPERATURE: return "TEMPERATURE";
        case PAGE_HUMIDITY:    return "HUMIDITY";
        case PAGE_LIGHT:       return "LIGHT";
        case PAGE_MOTION:      return "MOTION";
        default:               return "UNKNOWN";
    }
}
