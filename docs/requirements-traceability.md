# Requirements Traceability Matrix — BCA182 Room Monitoring System

**Project:** Real-Time Multisensor Room Monitoring System  
**Platform:** STM32F103C8T6 (Blue Pill) + native FreeRTOS + STM32Cube HAL (PlatformIO)  
**Simulation:** Wokwi

This matrix maps every functional requirement of Laboratory Activity No. 1 to its
implementation and to the evidence that verifies it.

---

## Traceability Matrix

| Req ID | Requirement | Implementation (file → function) | Verification | Status |
|--------|-------------|----------------------------------|--------------|--------|
| FR-01 | Periodically obtain temperature | `src/app/tasks/sensor_task.c` → `SensorTask()`; `src/app/hal/dht22.c` → `DHT22_Read()` | Unit: `test_temperature` (temperature bounds); Functional: FT-01 | Unit verified / FT to record |
| FR-02 | Periodically obtain humidity | `src/app/tasks/sensor_task.c` → `SensorTask()`; `DHT22_GetHumidity()` | Functional: FT-02 | FT to record |
| FR-03 | Monitor ambient-light level | `src/app/tasks/sensor_task.c` → `SensorTask()`; `src/app/hal/ldr.c` → `LDR_Read()` | Functional: FT-03 | FT to record |
| FR-04 | Detect motion via PIR | `src/app/tasks/motion_task.c` → `MotionTask()`; `src/app/hal/pir.c` → `PIR_GetState()` | Functional: FT-08, FT-10 | FT to record |
| FR-05 | Display one measurement at a time | `src/app/tasks/display_task.c` → `DisplayTask()` | Functional: FT-01–FT-03 | FT to record |
| FR-06 | Encoder switches Temperature/Humidity/Light/Motion | `src/app/tasks/input_task.c` → `InputTask()`; `src/app/logic/display_page.c` → `DisplayPage_Next()`/`DisplayPage_Previous()` | Unit: `test_navigation` (10); Functional: FT-04, FT-05 | Unit verified / FT to record |
| FR-07 | Buzzer activates outside normal temperature range | `src/app/logic/temperature.c` → `EvaluateTemperature()`; `src/app/tasks/alarm_task.c` → `AlarmTask()`; `src/app/logic/alarm.c` → `Alarm_Update()` | Unit: `test_temperature`; Functional: FT-06, FT-07 | Unit verified / FT to record |
| FR-08 | Support ACTIVE and INACTIVE states | `src/app/logic/state_machine.c` → `StateMachine_Update()`; `src/app/tasks/state_task.c` → `StateTask()` | Unit: `test_state_machine` (8); Functional: FT-08 | Unit verified / FT to record |
| FR-09 | Enter INACTIVE after no motion for a set duration | `src/app/logic/state_machine.c` (`timeout_ms`), `StateTask()`, `DisplayTask()` blanks OLED | Unit: `test_state_machine`; Functional: FT-09 | Unit verified / FT to record |
| FR-10 | Motion returns the system to ACTIVE | `src/app/tasks/state_task.c` → `StateTask()` (via `EVENT_MOTION_BIT`) | Unit: `test_state_machine`; Functional: FT-10 | Unit verified / FT to record |

### Required FreeRTOS mechanisms

| Mechanism | Where | Role |
|-----------|-------|------|
| Multiple tasks + explicit priorities | `src/main.c` (6 × `xTaskCreate`) | See `README.md` task table |
| `vTaskDelayUntil()` | `src/app/tasks/sensor_task.c` | Drift-free 1 s sampling |
| Queue | `alarm_queue`, `display_queue`, `display_page_queue` | Sample and page transport |
| Mutex | `src/drivers/uart_mutex.c` (`uart_mutex`) | Serialize UART messages |
| Event group | `src/main.c` (`event_group`) | Motion → state → display signaling |
| Task notification | `src/main.c` ISR handlers; `MotionTask()`, `InputTask()` | Fast wake-up on PIR/encoder EXTI |
| State machine | `src/app/logic/state_machine.c` | ACTIVE / INACTIVE |

---

## Implementation Notes

- **FR-01/FR-02/FR-03** are produced by one periodic task (SensorTask) because a
  single I²C / ADC / one-wire read cycle yields one coherent sample. The sample is
  published to two independent queues so the alarm and display paths each receive
  every reading.
- **FR-06** is implemented as pure logic (`DisplayPage_Next`/`Previous`) so the
  wraparound behaviour is unit tested, with `InputTask` only wiring the encoder
  events to the queue.
- **FR-07** uses the laboratory limits 18 °C (low) and 30 °C (high). A failed
  DHT22 read is published as `NAN` and is explicitly ignored by the alarm so a
  sensor fault cannot raise a false alarm.
- **FR-08–FR-10** are centralized in `StateTask`, which owns the
  `StateMachine_t`. The state is published through `EVENT_STATE_ACTIVE_BIT`; the
  `DisplayTask` consumes it and powers the OLED off while INACTIVE.

---

## Verification Coverage Summary

| Suite | Tests | Requirements covered |
|-------|-------|----------------------|
| `test_temperature` | 15 | FR-07 (and threshold logic used by FR-01) |
| `test_navigation` | 10 | FR-06 |
| `test_state_machine` | 8 | FR-08, FR-09, FR-10 |
| **Total** | **33** | 100 % pass (`pio test -e native`) |

Functional tests FT-01 … FT-10 are defined in
[`functional-verification.md`](functional-verification.md). Their observed
results must be recorded from a live Wokwi run.
