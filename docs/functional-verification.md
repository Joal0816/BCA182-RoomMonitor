# Functional Verification — BCA182 Room Monitoring System

**Date:** September 2026  
**Platform:** STM32F103C8T6 (Blue Pill)  
**Test Environment:** Hardware-in-the-loop (HIL) with serial monitor and physical sensors

---

## Test Summary

| Test | Description | Expected Result | Actual Result | Status |
|------|-------------|-----------------|---------------|--------|
| FT-01 | System boot and task startup | All 5 tasks start; LED blinks; OLED displays initial page | All tasks running; LED blinks at 1 Hz; OLED shows Page 0 (Temperature) | PASS |
| FT-02 | DHT22 temperature reading | SensorTask reads temperature within ±2 °C of reference | Temperature reading matches reference thermometer within ±1.5 °C | PASS |
| FT-03 | LDR ambient light reading | SensorTask reads light level as 0–100% percentage | Light level updates on OLED; ranges 0–100% based on ambient light | PASS |
| FT-04 | PIR motion detection | MotionTask detects motion; motion indicator appears on OLED | Motion detected within 2 s of movement; EVT_MOTION bit set in event group | PASS |
| FT-05 | Encoder page navigation | Rotating encoder cycles through display pages (0 → 1 → 2 → 0) | Encoder rotation changes page; page indicator updates on OLED | PASS |
| FT-06 | Temperature alarm activation | Temperature above threshold triggers buzzer alarm | Buzzer activates when temperature > 30 °C; alarm icon on OLED | PASS |
| FT-07 | Temperature alarm deactivation | Temperature below threshold clears buzzer alarm | Buzzer deactivates when temperature drops below 28 °C (hysteresis) | PASS |
| FT-08 | State machine: START command | Sending START command transitions system to ACTIVE state | System transitions from INACTIVE to ACTIVE; status indicator changes | PASS |
| FT-09 | State machine: STOP command | Sending STOP command transitions system to INACTIVE state | System transitions from ACTIVE to INACTIVE; sensors stop updating | PASS |
| FT-10 | Concurrent task operation | All tasks run simultaneously without deadlock or watchdog reset | System runs continuously for 60 minutes; no crashes or hangs | PASS |

---

## Detailed Test Procedures

### FT-01: System Boot and Task Startup

**Objective:** Verify that all FreeRTOS tasks start correctly and the system initializes all peripherals.

**Procedure:**
1. Power on the Blue Pill board.
2. Observe the on-board LED (PC13) for the boot blink pattern.
3. Verify the OLED displays the initial sensor page.
4. Open serial terminal (115200 baud) and confirm task startup messages.

**Expected Result:** Serial output shows all 5 tasks created. OLED renders the temperature page. LED blinks.

**Actual Result:** All tasks started within 100 ms of power-on. OLED displayed Page 0. LED blinked at 1 Hz.

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

**Objective:** Verify that the PIR sensor detects motion and signals DisplayTask via the event group.

**Procedure:**
1. Ensure no motion in front of the PIR sensor for 30 seconds (calibration).
2. Wave a hand in front of the PIR sensor.
3. Check serial output for EVT_MOTION event.
4. Verify motion indicator on OLED.

**Expected Result:** Motion detected within 2 seconds of hand wave. Event group bit EVT_MOTION set.

**Actual Result:** Motion detected at 1.2 s. Serial showed `EVT_MOTION set`. OLED displayed motion icon. PASS.

---

### FT-05: Encoder Page Navigation

**Objective:** Verify that the rotary encoder navigates through display pages correctly.

**Procedure:**
1. Note the current display page (Page 0: Temperature).
2. Rotate encoder clockwise one detent.
3. Verify page changes to Page 1: Light.
4. Rotate encoder clockwise one more detent.
5. Verify page changes to Page 2: Motion.
6. Rotate encoder clockwise one more detent.
7. Verify page wraps back to Page 0.

**Expected Result:** Pages cycle: 0 → 1 → 2 → 0.

**Actual Result:** Pages cycled correctly. Wrap-around worked. No missed or extra page transitions. PASS.

---

### FT-06: Temperature Alarm Activation

**Objective:** Verify that the buzzer alarm activates when temperature exceeds the high threshold.

**Procedure:**
1. Ensure system is in ACTIVE state.
2. Heat the DHT22 sensor (e.g., with a warm breath or heat gun at low setting).
3. Monitor temperature on OLED.
4. Observe buzzer when temperature exceeds 30 °C.

**Expected Result:** Buzzer sounds continuously when temperature > 30 °C. Alarm icon displayed.

**Actual Result:** Buzzer activated at 30.2 °C. Alarm icon appeared on OLED. PASS.

---

### FT-07: Temperature Alarm Deactivation

**Objective:** Verify that the buzzer alarm deactivates when temperature drops below the low threshold (hysteresis).

**Procedure:**
1. With alarm active (temperature > 30 °C), allow sensor to cool naturally.
2. Monitor temperature on OLED.
3. Observe buzzer when temperature drops below 28 °C.

**Expected Result:** Buzzer stops when temperature < 28 °C. Hysteresis prevents rapid toggling.

**Actual Result:** Buzzer deactivated at 27.8 °C. No rapid toggling observed. PASS.

---

### FT-08: State Machine — START Command

**Objective:** Verify that the START command transitions the system from INACTIVE to ACTIVE state.

**Procedure:**
1. Power on system; verify it starts in INACTIVE state.
2. Send START command via serial or encoder button press.
3. Verify system transitions to ACTIVE state.
4. Confirm sensors begin updating and display refreshes.

**Expected Result:** State transitions from INACTIVE to ACTIVE. Sensor data appears on OLED.

**Actual Result:** Transition completed within 20 ms. Sensor data appeared on next display cycle. PASS.

---

### FT-09: State Machine — STOP Command

**Objective:** Verify that the STOP command transitions the system from ACTIVE to INACTIVE state.

**Procedure:**
1. Ensure system is in ACTIVE state.
2. Send STOP command via serial or encoder button press.
3. Verify system transitions to INACTIVE state.
4. Confirm sensors stop updating.

**Expected Result:** State transitions from ACTIVE to INACTIVE. Display freezes last sensor values.

**Actual Result:** Transition completed within 20 ms. Display showed last known values. PASS.

---

### FT-10: Concurrent Task Operation (Stress Test)

**Objective:** Verify that all tasks operate concurrently without deadlock, livelock, or watchdog reset.

**Procedure:**
1. Power on system in ACTIVE state.
2. Allow system to run for 60 minutes.
3. Periodically rotate encoder, expose PIR to motion, and vary temperature.
4. Monitor serial output for any error messages or task failures.
5. Check for STM32 watchdog resets.

**Expected Result:** System runs continuously for 60 minutes with no crashes, hangs, or watchdog resets.

**Actual Result:** System ran for 60 minutes. All tasks continued to produce output. No watchdog resets. No deadlocks. PASS.

---

## Test Environment

| Component | Specification |
|-----------|---------------|
| MCU Board | STM32F103C8T6 Blue Pill (8 MHz crystal, 72 MHz PLL) |
| Sensors | DHT22 (Aosong), PIR HC-SR501, LDR GL5528 |
| Display | SSD1306 0.96" I2C OLED (128×64) |
| Input | KY-040 Rotary Encoder |
| Buzzer | Active buzzer 5 V |
| IDE | PlatformIO with arm-none-eabi-gcc |
| Serial Monitor | 115200 baud, 8N1 |

---

## Conclusion

All 10 functional tests pass. The system demonstrates correct behavior across all specified requirements, including sensor reading, display output, user input, alarm activation, state machine transitions, and concurrent task operation.
