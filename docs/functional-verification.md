# Functional Verification — BCA182 Room Monitoring System

**Platform:** STM32F103C8T6 (Blue Pill)  
**Environment:** Wokwi simulation (STM32 firmware built with PlatformIO)  
**Firmware under test:** `.pio/build/bluepill_f103c8/firmware.elf`

---

## How to run the verification

1. `pio run -e bluepill_f103c8` — build the firmware.
2. Open the project in VS Code with the Wokwi extension, start the simulation
   (the root `diagram.json` and `wokwi.toml` are used automatically).
3. Open the Wokwi serial terminal (115200 baud) to observe the RTOS logs.
4. Apply each stimulus below and record the **actual** observation before marking
   a test PASS or FAIL.

> The RTOS logs identify each producer, e.g. `[SENSOR]`, `[MOTION]`, `[INPUT]`,
> `[STATE]`, `[ALARM]`, `[DISPLAY]`, which makes the inter-task behaviour visible
> during the tests.

---

## Verification Record

| Test ID | Input / Stimulus | Expected Result | Actual Observation | Result |
|---------|------------------|-----------------|--------------------|--------|
| FT-01 | Change DHT22 temperature | Displayed temperature updates | *record* | ☐ |
| FT-02 | Change DHT22 humidity | Displayed humidity updates | *record* | ☐ |
| FT-03 | Change photoresistor light level | Displayed light value changes | *record* | ☐ |
| FT-04 | Rotate encoder clockwise | Next page selected (Temp→Hum→Light→Motion→Temp) | *record* | ☐ |
| FT-05 | Rotate encoder counter-clockwise | Previous page selected (wraps Motion→Temp) | *record* | ☐ |
| FT-06 | Set temperature above 30 °C | Alarm activates (buzzer) | *record* | ☐ |
| FT-07 | Return temperature to normal (18–30 °C) | Alarm stops | *record* | ☐ |
| FT-08 | Trigger PIR motion | System is ACTIVE (OLED on) | *record* | ☐ |
| FT-09 | Allow 15 s inactivity | System becomes INACTIVE (OLED blank) | *record* | ☐ |
| FT-10 | Trigger PIR while INACTIVE | System returns to ACTIVE (OLED on) | *record* | ☐ |

A test is marked **PASS** only after the actual behaviour has been recorded above.

---

## Detailed Procedures

### FT-01 — Temperature display
1. Select the Temperature page (rotate the encoder as needed).
2. In the Wokwi DHT22 part, change the `temperature` attribute (or drag the
   control) to a new value in the 18–30 °C band.
3. Observe the OLED and the `[SENSOR]` log line.
4. **Expected:** the displayed temperature follows the new value within one
   sampling period (1 s).

### FT-02 — Humidity display
1. Select the Humidity page.
2. Change the DHT22 `humidity` attribute.
3. **Expected:** the displayed humidity and progress bar update.

### FT-03 — Light display
1. Select the Light page.
2. Drag the photoresistor control to change illumination.
3. **Expected:** the displayed raw ADC value (0–4095) and proportional bar change.

### FT-04 / FT-05 — Encoder navigation
1. Start on the Temperature page.
2. Rotate the encoder clockwise one detent at a time.
3. **Expected (CW):** Temperature → Humidity → Light → Motion → Temperature.
4. Rotate counter-clockwise.
5. **Expected (CCW):** the reverse sequence with wraparound.
6. The `[INPUT]` log prints the new page name on every detent.

### FT-06 / FT-07 — Temperature alarm
1. Set the DHT22 temperature above 30 °C.
2. **Expected:** the `[ALARM]` log reports `HIGH` and the buzzer sounds.
3. Return the temperature to within 18–30 °C.
4. **Expected:** the buzzer stops and the alarm clears.
5. Repeat below 18 °C to confirm the `LOW` alarm.

### FT-08 / FT-09 / FT-10 — State machine
1. Trigger the PIR. **Expected:** `[STATE] ACTIVE` and the OLED turns on (FT-08).
2. Leave the PIR idle for 15 s. **Expected:** `[STATE] INACTIVE`,
   `[DISPLAY] OLED blanked` (FT-09).
3. Trigger the PIR again. **Expected:** `[STATE] ACTIVE`,
   `[DISPLAY] OLED restored` (FT-10).

---

## Notes on Simulation Fidelity

Wokwi's STM32 runtime approximates real hardware. Confirm which peripherals your
Wokwi version renders (the SSD1306 panel and the serial terminal are part of the
circuit in `diagram.json`). If a peripheral is not rendered by your simulator
build, record that as the observed result rather than reporting a PASS, and rely
on the host unit tests (`docs/laboratory-report.md`, Section 5) for that logic.
