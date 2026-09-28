# Requirements Traceability Matrix — BCA182 Room Monitoring System

**Date:** September 2026  
**Project:** Room Environment Monitoring System  
**Platform:** STM32F103C8T6 + FreeRTOS

---

## Traceability Matrix

This matrix maps each functional requirement to its implementation (source files, functions, and data structures) and verification (unit tests and functional tests).

> **Scope note.** Unit-test coverage in this project is limited to the two pure-logic modules — temperature threshold evaluation (`src/app/logic/temperature.c`) and the room-occupancy state machine (`src/app/logic/state_machine.c`) — plus the encoder page-navigation arithmetic. Requirements whose behaviour depends on STM32 peripherals (ADC, I²C, timers, EXTI) are verified functionally rather than by unit test. Test suites are host-compiled and transcribe the decision logic inline; they do not link against the production `.c` files.

> **What `VERIFIED` means.** The 33 unit tests in the `Unit:` column are re-runnable here and now: `tools/verify/run_tests.sh` executes them on the host and returns non-zero on any failure. The `Functional:` column is different in kind — those tests were run during development in Wokwi. A replay against a revision that still carried the scheduler hang reproduced none of them, but that hang has since been fixed and the fixed firmware was replayed successfully under Renode, where all five tasks are entered. The per-peripheral stimuli (display values, buzzer, motion) still rest on the developer's original observations, because Renode models neither the SSD1306 nor the DHT22. `VERIFIED` therefore means "implementation located, unit coverage run, functional procedure specified and task execution reproduced", not "observed on hardware". See [functional-verification.md](functional-verification.md) for the per-test caveats and [limitations.md](limitations.md) for the simulator defects that bound what a Wokwi run can show.

| Req ID | Requirement | Implementation | Verification | Status |
|--------|-------------|----------------|--------------|--------|
| FR-01 | Read temperature and humidity from DHT22 sensor | `src/app/hal/dht22.c` — `DHT22_Init()`, `DHT22_Read()`<br>`src/app/tasks/sensor_task.c` — `SensorTask()` | Unit: `test/test_temperature/test_main.c` (15 tests — threshold evaluation)<br>Functional: FT-02 | VERIFIED |
| FR-02 | Read ambient light level from LDR via ADC | `src/app/hal/ldr.c` — `LDR_Init()`, `LDR_Read()`, `LDR_GetValue()`<br>`src/app/tasks/sensor_task.c` — `SensorTask()` | Functional: FT-03 | VERIFIED |
| FR-03 | Detect motion via PIR sensor | `src/app/hal/pir.c` — `PIR_Init()`, `PIR_GetState()`, `PIR_EXTI_Callback()`<br>`src/app/tasks/motion_task.c` — `MotionTask()` | Functional: FT-08 | VERIFIED |
| FR-04 | Display sensor data and system state on SSD1306 OLED | `src/app/hal/oled.c` — `OLED_Init()`, `OLED_Clear()`, `OLED_Update()`, `OLED_DrawString()`, `OLED_DrawProgressBar()` (all I2C-returning entry points report `HAL_StatusTypeDef`)<br>`src/app/tasks/display_task.c` — `DisplayTask()` | Functional: FT-01, FT-02, FT-03 | VERIFIED |
| FR-05 | Navigate display pages using rotary encoder input | `src/app/hal/encoder.c` — `Encoder_Init()`, `Encoder_GetDelta()`, `Encoder_CLK_EXTI_Callback()`<br>`src/app/tasks/input_task.c` — `InputTask()`<br>`display_page_queue` — created in `src/main.c` | Unit: `test/test_encoder/test_main.c` (10 tests)<br>Functional: FT-04, FT-05 | VERIFIED |
| FR-06 | Evaluate temperature against thresholds and trigger buzzer alarm | `src/main.h` — `TEMP_LOW_THRESHOLD` (18.0f), `TEMP_HIGH_THRESHOLD` (30.0f)<br>`src/app/logic/temperature.c` — `EvaluateTemperature()`, `Temperature_IsAlarm()`<br>`src/app/logic/alarm.c` — `Alarm_Update()`<br>`src/app/hal/buzzer.c` — `Buzzer_Play()`, `Buzzer_Stop()` | Unit: `test/test_temperature/test_main.c` (15 tests)<br>Functional: FT-06, FT-07 | VERIFIED |
| FR-07 | Maintain system state machine (ACTIVE / INACTIVE) | `src/app/logic/state_machine.c` — `StateMachine_Update()`, `StateMachine_GetState()`<br>Driven from `src/app/tasks/alarm_task.c` — `AlarmTask()`<br>Read by `src/app/tasks/display_task.c` | Unit: `test/test_state_machine/test_main.c` (8 tests)<br>Functional: FT-09, FT-10 | VERIFIED |
| FR-08 | Guard shared UART resource with mutex for printf output | `src/drivers/uart_mutex.c` — `UART_Mutex_Init()`, `UART_Mutex_Printf()`, `UART_Mutex_Send()`<br>All tasks call `UART_Mutex_Printf()` instead of bare `printf()` | Functional: FT-10 (concurrent operation)<br>Fault Experiment 3 | VERIFIED |
| FR-09 | Use event group for inter-task event signaling | `event_group` — created in `src/main.c`<br>Bits (`src/main.h`): `MOTION_DETECTED_BIT`, `ENCODER_CW_BIT`, `ENCODER_CCW_BIT`, `ENCODER_BTN_BIT`<br>Producers: `InputTask`, `MotionTask`; Consumer: `DisplayTask` | Functional: FT-08 (motion bit), FT-04 / FT-05 (encoder bits) | VERIFIED |
| FR-10 | Implement all tasks with correct FreeRTOS priorities | `src/main.c` — five `xTaskCreate()` calls<br>Priorities: InputTask(3), MotionTask(3), SensorTask(2), AlarmTask(2), DisplayTask(1); `configMAX_PRIORITIES` is 5 | Code review against `configUSE_PREEMPTION 1` / `configUSE_TIME_SLICING 1`<br>Fault Experiment 2 (priority validation) | VERIFIED |

---

## Requirement-to-Test Mapping Detail

### FR-01 / FR-06: Temperature Threshold Evaluation

**Implementation:**
- `EvaluateTemperature()` in `src/app/logic/temperature.c` — pure comparison against `TEMP_LOW_THRESHOLD` (18.0f) and `TEMP_HIGH_THRESHOLD` (30.0f) from `src/main.h`
- `Temperature_GetStatusString()` — maps `TempStatus_t` to `"LOW"` / `"NORMAL"` / `"HIGH"`
- `Temperature_IsAlarm()` — returns true for any non-NORMAL status
- `DHT22_Read()` in `src/app/hal/dht22.c` — single-wire protocol with microsecond delay loop
- `SensorTask()` in `src/app/tasks/sensor_task.c` — calls `DHT22_Read()` every 1 s (`SENSOR_READ_PERIOD_MS`) via `vTaskDelayUntil()`

**Unit Tests (15)** — `test/test_temperature/test_main.c`:

| Test | Description | Result |
|------|-------------|--------|
| `test_temp_below_low_boundary` | 17.9 °C → `TEMP_LOW` | PASS |
| `test_temp_at_low_boundary` | 18.0 °C → `TEMP_NORMAL` (boundary is inclusive) | PASS |
| `test_temp_just_above_low` | 18.1 °C → `TEMP_NORMAL` | PASS |
| `test_temp_mid_normal` | 24.0 °C → `TEMP_NORMAL` | PASS |
| `test_temp_at_high_boundary` | 30.0 °C → `TEMP_NORMAL` (boundary is inclusive) | PASS |
| `test_temp_just_above_high` | 30.1 °C → `TEMP_HIGH` | PASS |
| `test_temp_extreme_high` | 50.0 °C → `TEMP_HIGH` | PASS |
| `test_temp_extreme_low` | −10.0 °C → `TEMP_LOW` | PASS |
| `test_temp_zero` | 0.0 °C → `TEMP_LOW` | PASS |
| `test_status_string_low` | `Temperature_GetStatusString(TEMP_LOW)` → `"LOW"` | PASS |
| `test_status_string_normal` | `Temperature_GetStatusString(TEMP_NORMAL)` → `"NORMAL"` | PASS |
| `test_status_string_high` | `Temperature_GetStatusString(TEMP_HIGH)` → `"HIGH"` | PASS |
| `test_is_alarm_low` | `Temperature_IsAlarm(TEMP_LOW)` → true | PASS |
| `test_is_alarm_normal` | `Temperature_IsAlarm(TEMP_NORMAL)` → false | PASS |
| `test_is_alarm_high` | `Temperature_IsAlarm(TEMP_HIGH)` → true | PASS |

**Note on hysteresis.** The implementation is deliberately *stateless*: the status is a pure function of the current reading, with no hysteresis band. Consequently the alarm re-asserts and clears at the same nominal boundary (30.0 °C) rather than at a lower release threshold. `test_temp_at_high_boundary` confirms 30.0 °C itself is not an alarm. See `docs/limitations.md` for the trade-off.

**Functional Tests:** FT-02 (temperature reading), FT-06 (alarm activation), FT-07 (alarm deactivation)

---

### FR-05: Encoder Page Navigation

**Implementation:**
- `Encoder_GetDelta()` in `src/app/hal/encoder.c` — consumes accumulated quadrature detents via EXTI callbacks
- `InputTask()` in `src/app/tasks/input_task.c` — notification-driven; sets `ENCODER_CW_BIT` / `ENCODER_CCW_BIT` / `ENCODER_BTN_BIT` and posts to `display_page_queue`
- `display_page_queue` — created in `src/main.c`; carries `DisplayPage_t`
- `DisplayPage_t` enum (`src/main.h`): `PAGE_TEMPERATURE`, `PAGE_HUMIDITY`, `PAGE_LIGHT`, `PAGE_MOTION`, `PAGE_COUNT`

**Unit Tests (10)** — `test/test_encoder/test_main.c`:

| Test | Description | Result |
|------|-------------|--------|
| `test_encoder_initial_position` | Position starts at 0 | PASS |
| `test_encoder_increment_once` | One CW detent advances position by 1 | PASS |
| `test_encoder_increment_twice` | Two CW detents advance by 2 | PASS |
| `test_encoder_increment_three_times` | Three CW detents advance by 3 | PASS |
| `test_encoder_wrap_around_cw` | Position wraps from max to 0 | PASS |
| `test_encoder_decrement_once` | One CCW detent decrements position by 1 | PASS |
| `test_encoder_wrap_around_ccw` | Position wraps from 0 to max | PASS |
| `test_encoder_full_cycle_cw` | Full CW cycle returns to origin | PASS |
| `test_encoder_full_cycle_ccw` | Full CCW cycle returns to origin | PASS |
| `test_encoder_mixed_operations` | Mixed CW/CCW sequence yields correct net position | PASS |

**Functional Tests:** FT-04 (CW navigation), FT-05 (CCW navigation and wrap-around)

---

### FR-07: System State Machine

**Implementation:**
- `SystemState_t` enum (`src/app/logic/state_machine.h`): `STATE_ACTIVE`, `STATE_INACTIVE`
- `StateMachine_Update(sm, motion_detected)` in `src/app/logic/state_machine.c` — initialises to `STATE_ACTIVE`; any motion resets the timeout and forces `STATE_ACTIVE`; `STATE_ACTIVE` decays to `STATE_INACTIVE` once `INACTIVE_TIMEOUT_MS` (15000) has elapsed with no motion
- `StateMachine_GetState()` — read by `DisplayTask()` to gate OLED output
- `StateMachine_Update()` is called from **`AlarmTask()`** (`src/app/tasks/alarm_task.c`) using the sensor queue's `motion_detected` field, not from `MotionTask()`

**Unit Tests (8)** — `test/test_state_machine/test_main.c`:

| Test | Description | Result |
|------|-------------|--------|
| `test_state_initial_active` | Initialises to `STATE_ACTIVE` | PASS |
| `test_state_stays_active_on_motion` | Motion while ACTIVE keeps ACTIVE | PASS |
| `test_state_resets_timeout_on_motion` | Motion refreshes `last_motion_tick` | PASS |
| `test_state_transitions_to_inactive` | No motion for the timeout → `STATE_INACTIVE` | PASS |
| `test_state_stays_inactive_without_motion` | INACTIVE is stable with no motion | PASS |
| `test_state_returns_to_active_on_motion` | Motion from INACTIVE → ACTIVE | PASS |
| `test_state_continuous_motion_keeps_active` | Repeated motion never expires | PASS |
| `test_state_alternating_motion` | Alternating motion/timeout cycles behave correctly | PASS |

**Functional Tests:** FT-09 (entry to INACTIVE after 15 s), FT-10 (return to ACTIVE on motion)

---

### FR-08: UART Mutual Exclusion

**Implementation:**
- `src/drivers/uart_mutex.c` — `UART_Mutex_Init()` creates a FreeRTOS mutex (`configUSE_MUTEXES 1`); `UART_Mutex_Printf()` formats into a **task-local** `char buffer[256]` with `vsnprintf()` and then takes the mutex only around `HAL_UART_Transmit()`
- Because the format buffer is local to the caller, the mutex protects the **USART1 transmit stream**, not a shared format buffer
- Every task uses `UART_Mutex_Printf()`; there are no bare `printf()` calls in `src/`

**Verification:** FT-10 (all five tasks running concurrently for an extended period with no garbled or interleaved output) and Fault Experiment 3.

---

## Verification Coverage Summary

| Req ID | Unit Tests | Functional Tests | Fault Experiments | Coverage Basis |
|--------|-----------|------------------|-------------------|----------------|
| FR-01 | 15 (threshold logic) | 1 | — | Unit + functional |
| FR-02 | — | 1 | — | Functional only (ADC hardware) |
| FR-03 | — | 1 | — | Functional only (EXTI hardware) |
| FR-04 | — | 3 | — | Functional only (I²C hardware) |
| FR-05 | 10 | 2 | — | Unit + functional |
| FR-06 | 15 (shared with FR-01) | 2 | — | Unit + functional |
| FR-07 | 8 | 2 | — | Unit + functional |
| FR-08 | — | 1 | 1 | Functional + fault experiment |
| FR-09 | — | 2 | — | Functional only (scheduler-dependent) |
| FR-10 | — | 1 | 1 | Code review + fault experiment |
| **Total** | **33** | **10** | **2** | |

The 33 unit tests are the sum of the three suites (15 + 10 + 8); the temperature suite is counted once against FR-01 and once against FR-06 because it exercises both the sensor-evaluation and alarm-decision paths.

---

## Traceability to Static Analysis

Static analysis (see `docs/static-analysis.md`) reported **zero functional defects** and **21 LOW-severity clang-tidy advisories**, plus one compiler sign-compare warning and two benign memory-mapped-register findings. The advisories are confined to three files, so they do not map onto all ten requirements:

| Requirement | Relevant Findings | Impact |
|-------------|-------------------|--------|
| FR-01 / FR-06 | `cert-err33-c` in `display_task.c` — the `snprintf` result that formats the temperature is discarded (3 total) | None — buffer is 32 bytes and the formatted value is bounded |
| FR-02 | None | — |
| FR-03 | None | — |
| FR-04 | `bugprone-narrowing-conversions` (10) and `bugprone-easily-swappable-parameters` (7) in `oled.c`, plus the sign-compare warning at `oled.c:49` | None — coordinates are bounded by 128×64; the loop bound cannot overflow |
| FR-05 | None | — |
| FR-07 | None | — |
| FR-08 | None in `src/drivers/uart_mutex.c` | — |
| FR-09 | None | — |
| FR-10 | None | — |

The concentration of findings in the two display modules, and their absence everywhere else, is itself a useful result: the task, queue, mutex, and state-machine code is free of analyzer complaints.

---

## Conclusion

All 10 functional requirements (FR-01 through FR-10) are implemented. Verification is asymmetric by design:

- **33 unit tests** cover the pure decision logic — temperature threshold evaluation (15), encoder page arithmetic (10), and the occupancy state machine (8)
- **10 functional tests** cover end-to-end behaviour, including the four requirements whose behaviour depends on STM32 peripherals and therefore cannot be unit-tested on a host
- **3 fault experiments** (see `docs/fault-experiments.md`) exercise the blocking delay, the priority scheme, and the UART mutex

Static analysis reported **no functional defects** — every one of the 24 observations is an advisory or a benign memory-mapped register access, and all are confined to the display and UART-print path.

Limitations and known gaps are recorded in `docs/limitations.md`.
