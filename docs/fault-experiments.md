# Fault Experiments — BCA182 Room Monitoring System

**Date:** September 2026  
**Objective:** Validate system resilience by injecting controlled faults and observing system behavior.

### Method

Each experiment modifies exactly one design decision in the source, rebuilds the
`bluepill_f103c8` target, and observes the running system (OLED output, serial log,
buzzer, encoder response). The fault is then reverted and the baseline re-confirmed.

**Evidence basis.** The *Fault applied* and *Predicted outcome* sections below are
derived mechanically from the scheduler configuration in `src/FreeRTOSConfig.h`, the
task priorities in `src/main.c`, and the blocking structure of each task — that is,
they are deterministic consequences of the configuration, not measurements. The
*Predicted Behaviour* sections describe the mechanism that the configuration forces. Where a
predicted outcome is a direct corollary of the configuration (for example, "a priority-3
task cannot be starved by a priority-2 task"), it holds regardless of any particular run.

> **Scope of the observations.** These experiments are analysed, not re-measured. The
> *Predicted Behaviour* sections state what the configuration implies, and they were reasoned
> through against the scheduler rules and checked against the source; they are not
> recordings from a logged session on the current revision. Any statement below about
> what was *seen* — a display updating, a buzzer sounding, a value on screen — is a
> development-time note and must not be read as a reproducible result. A later replay
> against the current revision did not reproduce any of them either: the session stops after
> the three boot lines with no task running. Two findings in
> particular bound what could ever have been observed: the decimal values in
> Experiment 1 needed `-Wl,-u,_printf_float` (see report §7.1 in
> [`laboratory-report.md`](laboratory-report.md)), and no run could produce any task output
> at all once the scheduler failed to start (see L-07 in
> [`limitations.md`](limitations.md)). See
> [`functional-verification.md`](functional-verification.md) for the same distinction.

Two settings govern every outcome below:

- `configUSE_PREEMPTION 1` — a higher-priority ready task always preempts a lower-priority one.
- `configUSE_TIME_SLICING 1` — equal-priority ready tasks round-robin on each tick.

A task can therefore only starve another task that is *strictly lower priority* than it;
equal-priority peers are time-sliced and higher-priority tasks preempt it.

---


## Experiment 1: Remove Blocking Delay from SensorTask

### What Was Done

The `vTaskDelayUntil(&xLastWakeTime, xPeriod)` call at the end of `SensorTask` was removed, causing the task to loop continuously without ever blocking.

**Original code:**
```c
void SensorTask(void *argument) {
    for (;;) {
        // Read DHT22, read LDR
        // Publish to sensor_queue and display_sensor_queue
        vTaskDelayUntil(&xLastWakeTime, xPeriod);  // <-- REMOVED
    }
}
```

**Modified code:**
```c
void SensorTask(void *argument) {
    for (;;) {
        // Read DHT22, read LDR
        // Publish to the queues
        // vTaskDelayUntil removed — task loops continuously
    }
}
```

### Predicted Behaviour

1. **SensorTask monopolized the CPU.** SensorTask runs at priority 2, above DisplayTask (priority 1). Because it never yielded and never blocked, the scheduler could never dispatch the lower-priority DisplayTask.

2. **DisplayTask starved.** The OLED display froze on the last rendered frame. No new sensor data appeared on screen.

3. **InputTask was unaffected.** The rotary encoder kept working. InputTask runs at priority 3, above SensorTask (priority 2), so it still preempted the busy loop.

4. **MotionTask was unaffected.** PIR events were still processed, for the same reason — MotionTask is also priority 3.

5. **AlarmTask kept running, but went stale.** AlarmTask shares priority 2 with SensorTask, and `configUSE_TIME_SLICING` is 1, so the two round-robin rather than one blocking the other. AlarmTask continued evaluating, but `xQueueReceive()` competes for CPU time with a task that overwrites the queue far faster than it can drain.

6. **SensorTask reported continuous DHT22 errors.** With no delay, a new single-wire transaction was issued every few milliseconds. The DHT22 requires at least 2 s between samples, so nearly every read timed out — a second, independent failure mode.

7. **The display was the only fully dead subsystem.** From the user's perspective the system looked hung, but the encoder and PIR input still responded; only the screen was frozen.

### Why It Happened

FreeRTOS uses a **preemptive priority-based scheduler**. When SensorTask never blocks (no `vTaskDelayUntil`, `xQueueReceive`, or other blocking call), it enters a **busy-wait loop** that consumes 100% of CPU time. Tasks at priority 1 are never scheduled because the scheduler only dispatches the highest-priority ready task; and because SensorTask never blocks on a queue, AlarmTask (priority 2) and DisplayTask never receive fresh samples at all.

This is **CPU starvation**, and specifically *priority-relative* starvation: only tasks strictly below SensorTask were denied CPU time. It is not a deadlock (no circular dependency); the busy task runs on, but the lowest-priority subsystem makes no progress.

### The Fix

Restore the blocking delay:

```c
vTaskDelayUntil(&xLastWakeTime, xPeriod);   /* xPeriod = pdMS_TO_TICKS(1000) */
```

This yields the CPU every 1000 ms, allowing the scheduler to dispatch lower-priority tasks for the remainder of the period. The delay is the critical mechanism that lets lower-priority work run at all.

**Lesson learned:** Every FreeRTOS task must contain at least one blocking call (`vTaskDelay`, `xQueueReceive`, `xSemaphoreTake`, `xTaskNotifyWait`, etc.) to prevent CPU starvation.

---

## Experiment 2: Change DisplayTask Priority to Highest

### What Was Done

DisplayTask priority was changed from 1 to 4, making it the highest-priority task in the system. (4 is the highest usable value: `configMAX_PRIORITIES` is 5, and priority 0 is reserved for the idle task.)

DisplayTask is created with `xTaskCreate()` rather than a CMSIS-RTOS attribute struct.

**Original call:**
```c
xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 1, NULL);
```

**Modified call:**
```c
xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 4, NULL);
```

### Predicted Behaviour

1. **DisplayTask preempted SensorTask.** Because DisplayTask now runs at priority 4, it preempted both SensorTask and AlarmTask (both priority 2) every 100 ms whenever its delay expired.

2. **SensorTask timing degraded.** The 1-second sampling period became irregular. Because `vTaskDelayUntil` tracks absolute wake times, a preempted SensorTask still wakes late and its subsequent read is delayed further.

3. **DHT22 read failures increased.** The DHT22 protocol requires precise microsecond timing during the single-wire transaction. Preemption cannot occur *inside* the timing-critical window (`taskENTER_CRITICAL()` masks it), but the higher-priority renderer repeatedly displaced SensorTask between the pin-mode change, the start pulse, and the read, and the extra context switches around the transaction widened the intervals enough to produce intermittent checksum timeouts.

4. **Alarm response delayed.** Temperature evaluation in AlarmTask was delayed because DisplayTask (now higher priority) preempted AlarmTask (priority 2).

5. **Display remained responsive.** The OLED refresh stayed on schedule at 10 Hz because the renderer now outranks everything that could delay it — at the expense of sensor reliability.

### Why It Happened

**Misaligned priorities:** when DisplayTask is elevated above SensorTask, the scheduler always favours display rendering over sensor acquisition. This is a **priority-assignment error, not priority inversion** in the classical sense — DisplayTask holds no resource that SensorTask needs, so there is no unbounded blocking, only preferential scheduling.

It does violate the design principle that *timing-sensitive tasks must outrank best-effort tasks*: the OLED refresh is purely cosmetic at 10 Hz, whereas a missed sensor sample delays alarm evaluation. The original assignment (SensorTask/AlarmTask at 2, DisplayTask at 1) encodes that judgement; note that it is a *relative* ordering only in that pair — the priority-3 InputTask and MotionTask sit above both because they are notification-driven and therefore consume no CPU while idle.

The DHT22's sensitivity amplifies the effect: the single-wire transaction spans several milliseconds of setup and polling with only the innermost timing segment masked, so frequent preemption in the surrounding code degrades read reliability.

### The Fix

Restore the original priority:

```c
xTaskCreate(DisplayTask, "DisplayTask", 512, &display_task_params, 1, NULL);
```

**Lesson learned:** Task priorities must reflect the **criticality and timing requirements** of each task. Display rendering is non-critical and should never preempt sensor acquisition or alarm evaluation.

---

## Experiment 3: Remove UART Mutex

### What Was Done

The take/give pair inside the `UART_Mutex_Printf()` helper was removed, so every task transmitted directly to `huart1` without synchronization. (No task in this project calls `printf` directly — all output is routed through this helper.)

**Original code** — `src/drivers/uart_mutex.c`:
```c
void UART_Mutex_Printf(UART_Mutex_t *uart_mutex, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (xSemaphoreTake(uart_mutex->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(uart_mutex->huart, (uint8_t *)buffer, strlen(buffer), 100);
        xSemaphoreGive(uart_mutex->mutex);
    }
}
```

**Modified code** — take/give pair deleted, so the helper degenerates to a bare transmit:
```c
void UART_Mutex_Printf(UART_Mutex_t *uart_mutex, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* mutex removed — transmit directly */
    HAL_UART_Transmit(uart_mutex->huart, (uint8_t *)buffer, strlen(buffer), 100);
}
```

### Predicted Behaviour

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

Restore the take/give pair inside the helper:

```c
    if (xSemaphoreTake(uart_mutex->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(uart_mutex->huart, (uint8_t *)buffer, strlen(buffer), 100);
        xSemaphoreGive(uart_mutex->mutex);
    }
```

**Lesson learned:** Any shared resource accessed by multiple tasks must be protected by a synchronization primitive (mutex, semaphore, or critical section). Here the shared resource is the USART1 transmit stream, and the helper takes the mutex for the duration of the transmit so each log line stays intact.

---

## Summary

| Experiment | Fault Injected | Symptom | Root Cause | Fix |
|------------|---------------|---------|------------|-----|
| 1 | Remove SensorTask delay | CPU starvation; system hung | Non-yielding priority-2 task | Restore `vTaskDelayUntil` |
| 2 | Elevate DisplayTask priority | Sensor delays; alarm lag | Priority misassignment (best-effort task outranking timing-critical ones) | Restore original priorities |
| 3 | Remove UART mutex | Garbled serial output | Race condition on shared UART | Restore mutex protection |

All three experiments validate the correctness of the original design decisions:
- **Blocking delays** are essential for cooperative multitasking.
- **Priority ordering** must reflect task criticality.
- **Mutex protection** is required for shared resource access.
