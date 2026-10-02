#include "input_task.h"
#include "logic/display_page.h"

/*
 * InputTask (priority 3) - processes rotary-encoder navigation.
 *
 * The encoder EXTI line notifies this task directly (task notification), so
 * the task is event driven and blocks indefinitely when idle. Page transitions
 * use the hardware-independent DisplayPage_* logic that is unit tested.
 */
void InputTask(void *pvParameters) {
    InputTaskParams_t *params = (InputTaskParams_t *)pvParameters;
    DisplayPage_t current_page = PAGE_TEMPERATURE;
    static const char started[] = "[INPUT] Task entered\r\n";
    UART_Mutex_RawSend(params->uart_mutex, (const uint8_t *)started,
                       sizeof(started) - 1U);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int8_t delta = Encoder_GetDelta(params->encoder);

        if (delta > 0) {
            current_page = DisplayPage_Next(current_page);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] CW  -> %s\r\n",
                              DisplayPage_Name(current_page));
        } else if (delta < 0) {
            current_page = DisplayPage_Previous(current_page);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] CCW -> %s\r\n",
                              DisplayPage_Name(current_page));
        }

        if (Encoder_IsButtonPressed(params->encoder)) {
            Encoder_ClearButton(params->encoder);
            UART_Mutex_Printf(params->uart_mutex, "[INPUT] Button pressed\r\n");
        }

        xQueueOverwrite(params->display_queue, &current_page);
    }
}
