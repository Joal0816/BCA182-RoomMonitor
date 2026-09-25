# Deliberate FreeRTOS Fault Experiments — BCA182 Room Monitoring System

These experiments were performed as required by Part XVII of the laboratory
activity. Each fault was injected temporarily, observed in Wokwi, and then
reverted. The descriptions below match the **actual** implementation
(native FreeRTOS API, `vTaskDelayUntil()`, `UART_Mutex_Printf()`).

Reference priorities: MotionTask 3, InputTask 3, SensorTask 2, AlarmTask 2,
StateTask 2, DisplayTask 1.

---

## Experiment 1 — Remove blocking from SensorTask

### What was done

The `vTaskDelayUntil(&xLastWakeTime, xPeriod)` call at the end of `SensorTask`
was removed, so the loop performed its finite work and immediately looped again
without ever blocking.

### What happened

1. `SensorTask` (priority 2) became permanently ready and consumed all CPU time
   that was not used by the higher-priority event tasks.
2. `MotionTask` and `InputTask` (priority 3) still ran when the PIR/encoder fired
   an interrupt, but the periodic work below priority 2 suffered badly.
3. With time slicing enabled, `AlarmTask` and `StateTask` (also priority 2)
   received short slices, but `DisplayTask` (priority 1) was starved: the OLED
   froze on its last frame and `[DISPLAY]` messages stopped.
4. Sensor sampling flooded the UART with back-to-back `[SENSOR]` lines, and the
   system looked unresponsive to the user.

### Why it happened

A task that never blocks never enters the **Blocked** state. FreeRTOS always
dispatching to the highest-priority ready task means such a task monopolises the
CPU and prevents lower-priority tasks from running, even though the scheduler is
still switching between equal-priority peers.

### Fix

Restore the blocking periodic delay:

```c
vTaskDelayUntil(&xLastWakeTime, xPeriod);
```

**Lesson:** every continuously executing task must perform finite work and then
block, wait or yield. FreeRTOS tasks are not polled loops.

---

## Experiment 2 — Give DisplayTask the highest priority

### What was done

`DisplayTask` was created with priority 4 (the maximum, since
`configMAX_PRIORITIES = 5`) instead of its designed priority 1.

### What happened

1. `DisplayTask` preempted every other task on each 100 ms refresh.
2. A full `OLED_Update()` transmits a 1 KB framebuffer over a 100 kHz I²C bus
   (~90 ms). At priority 4 this dominated the CPU during each refresh cycle.
3. `SensorTask` sampling became irregular and `AlarmTask` response latency
   increased, because both were repeatedly preempted.
4. The display itself was perfectly smooth — at the expense of the tasks that
   produce correct, timely data.

### Why it happened

Priority reflects scheduling urgency. Making cosmetic rendering the highest
priority inverts the intended order: the producer/decision tasks can no longer
meet their periods.

### Fix

Restore `DisplayTask` to priority 1:

```c
xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 1, NULL);
```

**Lesson:** give the highest priorities to the tasks with the tightest latency
requirements (event inputs), not to the tasks that merely run frequently.

---

## Experiment 3 — Remove the UART mutex

### What was done

The `xSemaphoreTake` / `xSemaphoreGive` pair inside `UART_Mutex_Printf()` was
removed so that tasks transmitted directly.

### What happened

1. Serial output became interleaved mid-line:

   ```
   [SENSOR] T=24.5C H=60.1[ALARM] Temp=31.2C Status=HIGH% L=1820 M=1
   ```

2. `HAL_UART_Transmit()` writes a multi-byte string; when another task starts a
   transmission before the first completes, the two messages share the wire.
3. No functional failure occurred — sensors, alarm and display were unaffected;
   only the human-readable log was corrupted.

### Why it happened

The UART transmit sequence is not atomic with respect to other tasks. Even
though the peripheral serialises bytes, the *message boundaries* are lost
without mutual exclusion.

### Fix

Restore the mutex in `UART_Mutex_Printf()`:

```c
if (xSemaphoreTake(uart_mutex->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    HAL_UART_Transmit(uart_mutex->huart, (uint8_t *)buffer, strlen(buffer), 100);
    xSemaphoreGive(uart_mutex->mutex);
}
```

**Lesson:** every shared resource accessed by more than one task must be
protected. Here the shared resource is the USART1 transmit path / log stream.

---

## Summary

| Experiment | Fault injected | Symptom | Root cause | Fix |
|------------|----------------|---------|------------|-----|
| 1 | Remove `vTaskDelayUntil()` from SensorTask | CPU starvation; frozen display | Task never blocks | Restore periodic delay |
| 2 | DisplayTask raised to priority 4 | Irregular sampling, alarm lag | Inverted priority order | Restore priority 1 |
| 3 | Remove UART mutex | Interleaved/garbled log lines | Shared UART without mutual exclusion | Restore mutex |
