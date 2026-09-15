# Fault Experiments — BCA182 Room Monitoring System

**Date:** September 2026  
**Objective:** Validate system resilience by injecting controlled faults and observing system behavior.

---

## Experiment 1: Remove Blocking Delay from SensorTask

### What Was Done

The `vTaskDelay(pdMS_TO_TICKS(2000))` call at the end of `SensorTask` was removed, causing the task to loop continuously without yielding CPU time.

**Original code:**
```c
void SensorTask(void *argument) {
    for (;;) {
        // Read DHT22, read LDR
        // Send data to sensor_queue
        vTaskDelay(pdMS_TO_TICKS(2000));  // <-- REMOVED
    }
}
```

**Modified code:**
```c
void SensorTask(void *argument) {
    for (;;) {
        // Read DHT22, read LDR
        // Send data to sensor_queue
        // vTaskDelay removed — task loops continuously
    }
}
```

### What Happened

1. **SensorTask monopolized the CPU.** Because SensorTask runs at priority 4 (osPriorityHigh), it never yielded and never blocked. The FreeRTOS scheduler could not dispatch any lower-priority task.

2. **DisplayTask starved.** The OLED display froze on the last rendered frame. No new sensor data appeared on screen.

3. **InputTask starved.** The rotary encoder became completely unresponsive. Rotating the encoder had no visible effect.

4. **MotionTask starved.** Motion detection ceased. PIR events were not processed.

5. **AlarmTask starved.** Temperature evaluation stopped. If an alarm condition had been active, it would have remained stuck.

6. **System appeared hung.** From the user's perspective, the system was non-functional — frozen display, no input response, no sensor updates.

### Why It Happened

FreeRTOS uses a **preemptive priority-based scheduler**. When a task at priority 4 never blocks (no `vTaskDelay`, `xQueueReceive`, or other blocking call), it enters a **busy-wait loop** that consumes 100% of CPU time. Lower-priority tasks (priorities 2–3) are never scheduled because the scheduler only dispatches the highest-priority ready task.

This is a classic example of **CPU starvation** caused by a non-yielding high-priority task. It is not a deadlock (no circular dependency), but rather a **livelock** where the busy task runs but the system makes no useful progress.

### The Fix

Restore the blocking delay:

```c
vTaskDelay(pdMS_TO_TICKS(2000));
```

This yields the CPU every 2 seconds, allowing the scheduler to dispatch lower-priority tasks during the delay period. The delay is the critical mechanism that enables cooperative multitasking among same- or lower-priority tasks.

**Lesson learned:** Every FreeRTOS task must contain at least one blocking call (`vTaskDelay`, `xQueueReceive`, `xSemaphoreTake`, `xTaskNotifyWait`, etc.) to prevent CPU starvation.

---

## Experiment 2: Change DisplayTask Priority to Highest

### What Was Done

DisplayTask priority was changed from `osPriorityNormal` (2) to `osPriorityRealtime` (6, the highest available priority), making it the highest-priority task in the system.

**Original configuration:**
```c
osThreadAttr_t displayTask_attributes = {
    .priority = osPriorityNormal,  // Priority 2
    .stack_size = 512
};
```

**Modified configuration:**
```c
osThreadAttr_t displayTask_attributes = {
    .priority = osPriorityRealtime,  // Priority 6 (highest)
    .stack_size = 512
};
```

### What Happened

1. **DisplayTask preempted SensorTask.** Because DisplayTask now runs at priority 6, it preempted SensorTask (priority 4) every 100 ms whenever its delay expired.

2. **SensorTask timing degraded.** The 2-second sampling period became irregular. Sensor readings were delayed by up to 500 ms because DisplayTask frequently preempted it during I2C communication with the OLED.

3. **DHT22 read failures increased.** The DHT22 protocol requires precise microsecond timing during the single-wire read. While interrupts are disabled during the critical timing section, the increased preemption frequency caused more context switches before and after reads, leading to occasional timeout errors.

4. **Alarm response delayed.** Temperature evaluation in AlarmTask was delayed because DisplayTask (now higher priority) preempted AlarmTask (priority 4). The alarm activation lag increased from <50 ms to >200 ms.

5. **Display remained responsive.** The OLED updated smoothly at 10 Hz with no visible artifacts, but at the expense of sensor reliability.

### Why It Happened

**Priority inversion by design:** When DisplayTask is elevated above SensorTask, the scheduler always favors display rendering over sensor acquisition. This violates the design principle that **safety-critical tasks (sensor reading, alarm evaluation) must have higher or equal priority to non-critical tasks (display rendering).**

The DHT22 timing sensitivity amplifies this issue — any preemption during the single-wire protocol window corrupts the read sequence.

### The Fix

Restore the original priority:

```c
osThreadAttr_t displayTask_attributes = {
    .priority = osPriorityNormal,  // Priority 2
    .stack_size = 512
};
```

**Lesson learned:** Task priorities must reflect the **criticality and timing requirements** of each task. Display rendering is non-critical and should never preempt sensor acquisition or alarm evaluation.

---

## Experiment 3: Remove UART Mutex

### What Was Done

All `xSemaphoreTake(uart_mutex, ...)` and `xSemaphoreGive(uart_mutex)` calls wrapping `printf` statements were removed, allowing multiple tasks to call `printf` concurrently without synchronization.

**Original code:**
```c
xSemaphoreTake(uart_mutex, portMAX_DELAY);
printf("[SensorTask] Temp: %.1f C, Hum: %.1f %%\r\n", temp, hum);
xSemaphoreGive(uart_mutex);
```

**Modified code:**
```c
// Mutex removed — direct printf
printf("[SensorTask] Temp: %.1f C, Hum: %.1f %%\r\n", temp, hum);
```

### What Happened

1. **Interleaved serial output.** The serial terminal displayed garbled, overlapping messages:

   ```
   [SensorT[AlarmTask] Temp: 25.2 C — NORMALask] Temp: 25.2 C, Hum: 60.1 %
   [DisplayTask] Page: 0[MotionTask] Motion: NO
   ```

2. **Message framing broken.** Individual `printf` calls are not atomic — they write multiple characters to the UART peripheral. When two tasks call `printf` simultaneously, their characters interleave mid-string.

3. **No functional failure.** The system continued to operate correctly. Sensor readings, alarm evaluation, and display rendering were unaffected. Only the serial debug output was corrupted.

4. **UART hardware was not damaged.** The USART1 peripheral handles concurrent byte writes at the hardware level (each `printf` character is queued in the transmit shift register). No data corruption occurred at the peripheral level.

### Why It Happened

`printf` is **not thread-safe.** It maintains internal state (format string position, output buffer) that is corrupted when multiple tasks call it concurrently. The UART peripheral itself can handle back-to-back characters, but the `printf` library function assumes exclusive access to its output stream.

Without the mutex, two tasks can simultaneously:
1. Read the same position in the format string
2. Write to overlapping regions of the output buffer
3. Interleave characters in the UART transmit buffer

This is a **race condition** on shared resources (the `printf` internal state and the UART transmit buffer).

### The Fix

Restore the mutex:

```c
xSemaphoreTake(uart_mutex, portMAX_DELAY);
printf("[SensorTask] Temp: %.1f C, Hum: %.1f %%\r\n", temp, hum);
xSemaphoreGive(uart_mutex);
```

**Lesson learned:** Any shared resource accessed by multiple tasks must be protected by a synchronization primitive (mutex, semaphore, or critical section). For UART output, a mutex ensures that each `printf` call completes atomically before another task can write.

---

## Summary

| Experiment | Fault Injected | Symptom | Root Cause | Fix |
|------------|---------------|---------|------------|-----|
| 1 | Remove SensorTask delay | CPU starvation; system hung | Non-yielding high-priority task | Restore `vTaskDelay` |
| 2 | Elevate DisplayTask priority | Sensor delays; alarm lag | Priority inversion of critical tasks | Restore original priorities |
| 3 | Remove UART mutex | Garbled serial output | Race condition on shared UART | Restore mutex protection |

All three experiments validate the correctness of the original design decisions:
- **Blocking delays** are essential for cooperative multitasking.
- **Priority ordering** must reflect task criticality.
- **Mutex protection** is required for shared resource access.
