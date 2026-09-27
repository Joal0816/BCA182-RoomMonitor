#include "input_task.h"

void InputTask(void *pvParameters) {
    InputTaskParams_t *params = (InputTaskParams_t *)pvParameters;
    DisplayPage_t current_page = PAGE_TEMPERATURE;

    /* Printed before the first blocking call so that "the task started" is
       distinguishable from "the task started but is parked". */
    UART_Mutex_Printf(params->uart_mutex, "[TASK] InputTask entered\r\n");

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int8_t delta = Encoder_GetDelta(params->encoder);

        if (delta > 0) {
            current_page = (DisplayPage_t)((current_page + 1) % PAGE_COUNT);
            xEventGroupSetBits(params->event_group, ENCODER_CW_BIT);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] Page CW -> %d\r\n", current_page);
        } else if (delta < 0) {
            current_page = (DisplayPage_t)((current_page + PAGE_COUNT - 1) % PAGE_COUNT);
            xEventGroupSetBits(params->event_group, ENCODER_CCW_BIT);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] Page CCW -> %d\r\n", current_page);
        }

        if (Encoder_IsButtonPressed(params->encoder)) {
            Encoder_ClearButton(params->encoder);
            xEventGroupSetBits(params->event_group, ENCODER_BTN_BIT);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] Button pressed\r\n");
        }

        xQueueOverwrite(params->display_queue, &current_page);
    }
}
