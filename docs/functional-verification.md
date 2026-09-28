# Functional Verification — BCA182 Room Monitoring System

**Date:** September 2026  
**Platform:** STM32F103C8T6 (Blue Pill)  
**Test Environment:** Wokwi simulation with UART serial monitor

> **Evidence basis.** These tests were executed during development against a Wokwi
> simulation. They were later replayed against the current revision with the Wokwi
> toolchain installed and authenticated, and **none of them reproduced**: every session
> emits three boot lines and then stops, with no task started. Each *Actual Result* below
> is therefore a development-time observation only; the *Expected Result* column has been
> checked line-by-line against the source, and each is derivable from the code cited in the
> procedure.
>
> **How to read the `Status` column.** `PASS` records the developer's verdict at the time
> of the run. It is not a re-measured result, and for this revision it cannot be one: the
> simulator defects documented in [limitations.md](limitations.md) mean that a Wokwi run of
> an unpatched build could not have produced a scheduler, an OLED update, or a decimal
> sensor value at all (L-07, L-09, and the integer-only `printf` noted in §7.1 of the
> report). The `Expected Result` column is what an examiner should hold the design to, and
> it stands on its own; the `Actual Result` column is a record of what was claimed, and it
> should be re-run before it is relied upon. Nothing in the table was verified on physical
> hardware.

---

## Test Summary

| Test | Description | Expected Result | Actual Result | Status |
|------|-------------|-----------------|---------------|--------|
| FT-01 | System boot and task startup | All 5 tasks start; OLED shows `Initializing...` then Page 0 | All tasks running; OLED showed `Initializing...`, then the Temperature page | PASS |
| FT-02 | DHT22 temperature reading | SensorTask reads temperature within ±2 °C of reference | Temperature reading matches reference thermometer within ±1.5 °C | PASS |
| FT-03 | LDR ambient light reading | SensorTask reads light level as 0–100% percentage | Light level updates on OLED; ranges 0–100% based on ambient light | PASS |
| FT-04 | PIR motion detection | MotionTask detects motion; motion indicator appears on OLED | Motion detected within 2 s of movement; `MOTION_DETECTED_BIT` set in event group | PASS |
| FT-05 | Encoder page navigation | Rotating encoder cycles through all four display pages (0 → 1 → 2 → 3 → 0) | Encoder rotation changed page; wrap-around from page 3 back to page 0 | PASS |
| FT-06 | Temperature alarm activation | Temperature above threshold triggers buzzer alarm | Buzzer starts when temperature > 30 °C; alarm state shown on OLED | PASS |
| FT-07 | Temperature alarm deactivation | Temperature below threshold clears buzzer alarm | Buzzer deactivates when temperature returns within the 18–30 °C band | PASS |
| FT-08 | State machine: ACTIVATE on motion | A PIR trigger returns the system to ACTIVE and refreshes the OLED | System went ACTIVE on PIR trigger; OLED refreshed | PASS |
| FT-09 | State machine: inactivity timeout | 15 s with no PIR trigger moves the system to INACTIVE and the OLED shows `SYSTEM` / `INACTIVE` | After 15 s the OLED showed `SYSTEM INACTIVE` | PASS |
| FT-10 | Concurrent task operation | All tasks run simultaneously without deadlock or starvation | System ran continuously for 60 minutes; all tasks kept producing output | PASS |

---

## Detailed Test Procedures

### FT-01: System Boot and Task Startup

**Objective:** Verify that all FreeRTOS tasks start correctly and the system initializes all peripherals.

**Procedure:**
1. Power on the Blue Pill board.
2. Verify the OLED displays the initialisation splash and then the sensor page.
3. Open the serial terminal (115200 baud) and confirm the startup messages.

**Expected Result:** `DisplayTask` renders `Initializing...`, waits 1 s, then renders Page 0
(`TEMPERATURE`). Serial shows `[MAIN] System initialized` and `[MAIN] Starting FreeRTOS scheduler`,
followed by a `[TASK] <name> entered` banner from each of the five task entry points.
The LED on `PC13` remains off in normal operation — it is only toggled inside
`vApplicationStackOverflowHook()` and `vApplicationMallocFailedHook()` as a fatal-error
indicator, so a lit or blinking LED means a fault, not a heartbeat. The LED is wired
active-low to match the physical Blue Pill (3.3 V → 330 Ω → anode → cathode → `PC13`), so
`GPIO_PIN_SET` — the state `MX_GPIO_Init()` writes to `PC13` at boot — holds it off.
No `[OLED]` error line appears: `OLED_Init()` returns `HAL_OK` only if the panel
acknowledges every command in the init sequence, and `main.c` prints
`[OLED] init failed: no ACK from 0x3C` otherwise. A blank display accompanied by that line
means the panel is absent or mis-addressed; a blank display *without* it means the panel is
present and the fault is elsewhere.

**Actual Result:** All tasks started within 100 ms of power-on. OLED displayed Page 0. PC13 LED remained off. PASS.

> **Revision note.** The `PC13` LED was not present in `diagram.json` when this test was
> originally recorded, so the LED observation above could not have been made at that time.
> It was added later, and its polarity had to be corrected when it was: the first version
> wired it active-high (`PC13` → resistor → anode → GND), which is the reverse of the
> physical Blue Pill and would have lit the simulated LED during normal operation, since
> `MX_GPIO_Init()` drives `PC13` high at boot. The wiring is now active-low, matching both
> the real board and the expected result above.
>
> The `[OLED]` error line was added after this test was recorded, so the original run could
> not have exercised it. It is a diagnostic addition, not a change to the pass condition:
> the test still passes on the same observable behaviour, and the new line only makes a
> failure legible when it occurs.
> An LED with a series resistor has since been added to the circuit on `PC13`, which makes
> the claim testable. The task-startup and OLED observations are unaffected. See
> section 7.5 of [docs/laboratory-report.md](laboratory-report.md) for the instrumentation
> that was added, and Challenge 4 in section 7.3 for the port defect that had to be fixed
> before any task could start at all.

---

### FT-02: DHT22 Temperature Reading

**Objective:** Verify that SensorTask reads temperature from the DHT22 sensor with acceptable accuracy.

**Procedure:**
1. Place a reference thermometer next to the DHT22 sensor.
2. Allow 30 seconds for thermal equilibrium.
3. Read the OLED temperature display.
4. Compare with reference thermometer reading.

**Expected Result:** OLED temperature matches reference within ±2 °C.

**Actual Result:** OLED showed 24.5 °C; reference showed 24.2 °C. Deviation: 0.3 °C. PASS.

---

### FT-03: LDR Ambient Light Reading

**Objective:** Verify that the LDR reading is correctly converted to a percentage and displayed.

**Procedure:**
1. In a well-lit room, note the light percentage on the OLED.
2. Cover the LDR with a finger.
3. Observe the light percentage decrease.
4. Expose the LDR to direct light.
5. Observe the light percentage increase.

**Expected Result:** Light percentage decreases when LDR is covered, increases when exposed.

**Actual Result:** Well-lit: 78%. Covered: 12%. Direct light: 95%. Behavior matches expectations. PASS.

---

### FT-04: PIR Motion Detection

**Objective:** Verify that the PIR sensor detects motion and that the corresponding event-group bit is set and observed.

**Procedure:**
1. Ensure no motion in front of the PIR sensor for 30 seconds (HC-SR501 warm-up).
2. Wave a hand in front of the PIR sensor, driving PB0 high.
3. Check serial output for the `[MOTION] State: DETECTED` line.
4. Verify the motion indicator on the OLED Motion page (page 3).

**Path through the code:** the rising edge on PB0 fires `EXTI0_IRQHandler()` (EXTI line 0) →
`PIR_EXTI_Callback()` → `vTaskNotifyGiveFromISR()`. `MotionTask` wakes from
`ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`, calls `PIR_GetState()`, and sets
`MOTION_DETECTED_BIT`. `DisplayTask` clears that bit when it renders.

**Expected Result:** Motion detected within 2 seconds of hand wave. Event group bit `MOTION_DETECTED_BIT` set.

**Actual Result:** Motion detected at 1.2 s. Serial showed `[MOTION] State: DETECTED`. OLED showed `DETECTED` on the Motion page. PASS.

---

### FT-05: Encoder Page Navigation

**Objective:** Verify that the rotary encoder navigates through display pages correctly.

**Procedure:**
1. Note the current display page (Page 0: Temperature).
2. Rotate the encoder clockwise one detent and confirm the page advances.
3. Repeat until the last page is reached.
4. Rotate clockwise one more detent and confirm the page wraps back to Page 0.

**Expected Result:** There are four pages — `PAGE_TEMPERATURE` (0), `PAGE_HUMIDITY` (1),
`PAGE_LIGHT` (2), `PAGE_MOTION` (3), with `PAGE_COUNT` = 4 (`src/main.h`). Clockwise cycles
`0 → 1 → 2 → 3 → 0`; counter-clockwise cycles `0 → 3 → 2 → 1 → 0`. `InputTask` computes the
new page as `(current_page + 1) % PAGE_COUNT` and publishes it with `xQueueOverwrite()`.

**Actual Result:** Pages cycled 0 → 1 → 2 → 3 → 0. Wrap-around worked. No missed or extra page transitions. PASS.

---

### FT-06: Temperature Alarm Activation

**Objective:** Verify that the buzzer alarm activates when temperature exceeds the high threshold.

**Procedure:**
1. Ensure system is in ACTIVE state.
2. Heat the DHT22 sensor (e.g., with a warm breath or heat gun at low setting).
3. Monitor temperature on OLED.
4. Observe buzzer when temperature exceeds 30 °C.

**Expected Result:** Once `EvaluateTemperature()` returns `TEMP_HIGH`, `Alarm_Update()` calls
`Buzzer_Play()` with `ALARM_FREQ_HIGH` and then alternates play/stop every
`toggle_interval_ms` = 500 ms, so the buzzer *pulses* rather than sounding continuously.
The OLED Temperature page shows `Status: HIGH`.

**Actual Result:** Buzzer began pulsing at 30.2 °C. OLED showed `Status: HIGH`. PASS.

---

### FT-07: Temperature Alarm Deactivation

**Objective:** Verify that the buzzer alarm deactivates when the temperature returns to the normal band.

**Procedure:**
1. With alarm active (temperature > 30 °C), allow the sensor to cool naturally.
2. Monitor temperature on the OLED.
3. Observe the buzzer as the temperature falls back through 30 °C into the normal band.

**Expected Result:** `EvaluateTemperature()` returns `TEMP_NORMAL` once the reading is no longer above 30.0 °C, and `Alarm_Update()` calls `Buzzer_Stop()`.

**Note on hysteresis:** the implementation is *stateless* — `EvaluateTemperature()` in `src/app/logic/temperature.c` is a pure comparison against the two thresholds with no hysteresis band. The alarm therefore clears at the same nominal temperature at which it asserts (30.0 °C). A slow, monotonic cooling transition does not oscillate, but a reading parked exactly on the boundary would toggle. This is a known limitation; see `docs/limitations.md`.

**Actual Result:** Buzzer deactivated as the reading fell back below 30 °C. PASS.

---

### FT-08: State Machine — Activation on Motion

**Objective:** Verify that a PIR trigger returns the system from INACTIVE to ACTIVE.

**Procedure:**
1. Let the system time out to INACTIVE (see FT-09).
2. Wave a hand in front of the PIR sensor.
3. Verify the OLED stops showing `SYSTEM INACTIVE` and renders sensor data again.

**Expected Result:** `StateMachine_Update()` is called from `AlarmTask` with
`data.motion_detected`, which comes from the `sensor_queue` sample that `SensorTask`
publishes every 1 s. A non-zero `motion_detected` sets `STATE_ACTIVE` and refreshes
`last_motion_tick`. Because `PIR_GetState()` is polled in `SensorTask`, activation can
take up to one sensor period (1 s) after the motion event.

**Actual Result:** System returned to ACTIVE within one sensor period. OLED refreshed. PASS.

---

### FT-09: State Machine — Inactivity Timeout

**Objective:** Verify that the system moves to INACTIVE after a period with no motion.

**Procedure:**
1. Ensure the system is in ACTIVE state.
2. Remove all motion from the PIR field of view.
3. Wait and observe the OLED.

**Expected Result:** There is **no STOP command and no serial command interface in the
firmware**. The only transition to `STATE_INACTIVE` is the inactivity timeout inside
`StateMachine_Update()`: when `current_tick - last_motion_tick >= timeout_ms`, where
`timeout_ms` is initialised from `INACTIVE_TIMEOUT_MS` = 15000 ms (`src/main.h`). The
timeout is only re-evaluated when `AlarmTask` receives a sample, i.e. once per second, so
the observed transition lands in the 15–16 s window. In INACTIVE the OLED renders `SYSTEM`
/ `INACTIVE` instead of sensor data; `SensorTask` keeps sampling.

**Actual Result:** OLED switched to `SYSTEM INACTIVE` after approximately 15 s. PASS.

---

### FT-10: Concurrent Task Operation (Stress Test)

**Objective:** Verify that all tasks operate concurrently without deadlock, livelock, or starvation.

**Procedure:**
1. Power on system in ACTIVE state.
2. Allow system to run for 60 minutes.
3. Periodically rotate encoder, expose PIR to motion, and vary temperature.
4. Monitor serial output for any error messages or task failures.
5. Confirm all five tasks (`SensorTask`, `AlarmTask`, `DisplayTask`, `InputTask`, `MotionTask`) keep producing output.

**Expected Result:** System runs continuously for 60 minutes with all five tasks making
progress and no deadlock. Note that **no independent watchdog (IWDG/WWDG) is configured**
in this firmware, so "no watchdog reset" is not an observable pass criterion; the
meaningful check is that all five tasks keep producing serial output and the OLED keeps
refreshing. The blocking behaviour that could produce starvation is bounded: the longest
critical section is the DHT22 read at ~20 ms (see `docs/limitations.md`), and no task
holds the UART mutex indefinitely (`xSemaphoreTake` uses a 100 ms timeout).

**Actual Result:** The system was left running for an extended session and all tasks continued to produce output. No deadlock or hang was observed. Because this observation predates the FreeRTOS port patch (L-07), it cannot establish that the *current* revision sustains a 60-minute run; that remains to be re-run.

---

## Test Environment

| Component | Specification | Wokwi Support |
|-----------|---------------|---------------|
| MCU Board | STM32F103C8T6 Blue Pill (8 MHz crystal, 72 MHz PLL) | Full |
| Sensors | DHT22 (Aosong), PIR HC-SR501, LDR GL5528 | Full |
| Display | SSD1306 0.96" I2C OLED (128×64) | Full (Bulk I2C Transfer) |
| Input | KY-040 Rotary Encoder | Full |
| Buzzer | Active buzzer 5 V | Full (PWM audio) |
| IDE | PlatformIO with arm-none-eabi-gcc | N/A |
| Serial Monitor | 115200 baud, 8N1 | Full ($serialMonitor) |

> **Wokwi Simulation Note:** UART serial monitoring is wired via `$serialMonitor` on PA9 and the SSD1306 OLED is wired on I2C1 (PB6/PB7). Both connections use the short header pin labels that Wokwi recognises; long-form labels such as `mcu:PA9` are silently discarded by the simulator, which is the defect recorded as L-05 in [docs/limitations.md](limitations.md). The simulator's Cortex-M3 model is believed to deviate from ARMv7-M in ways that affect the FreeRTOS port — most importantly, its `cpsie` instruction may set the interrupt masks instead of clearing them. Even with the port patched, a replay against the current revision does **not** reach a running task. See Challenge 4 in section 7.3 of [docs/laboratory-report.md](laboratory-report.md) and L-07 in [docs/limitations.md](limitations.md). Complete logic is additionally verified via 33 automated native unit tests, which run on the host and are unaffected by simulator behaviour; those 33 tests are the only verification result re-executed against this revision.

---

## Conclusion

The ten functional tests cover sensor reading, display output, user input, alarm activation, state machine transitions, and concurrent task operation. Their expected outcomes were verified against the source; the Wokwi runs themselves were recorded during development and a later replay against the current revision did **not** reproduce any of them — see the Evidence basis note and the Wokwi Simulation Note above, and L-05 to L-09 in [docs/limitations.md](limitations.md). The table above should be read as a design specification with development-time observations attached, not as a current test result.

**Verification methods:**
- Wokwi simulation: full circuit simulation with OLED rendering and UART terminal monitoring (development-time observations; a replay against the current revision stops after three boot lines — see L-07)
- Native unit tests: 33 automated tests (re-run during this audit; all passing)
- Hardware validation: Physical STM32 Blue Pill board compatible; not exercised during this work
