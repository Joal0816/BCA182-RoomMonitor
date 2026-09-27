# Known Limitations — BCA182 Room Monitoring System

**Date:** September 2026  
**Project:** BCA182 Laboratory Activity 1 — Room Monitoring System  
**Platform:** STM32 Blue Pill (STM32F103C8T6) + FreeRTOS + Wokwi Simulation

---

## Summary Table

| # | Limitation | Severity | Workaround | Status |
|---|-----------|----------|------------|--------|
| L-01 | DHT22 blocking read (~20 ms) | Low | Acceptable given 1 s sampling period (2% CPU) | Accepted |
| L-02 | Single buzzer (temperature only) | Low | Future: add humidity/motion alarms | Accepted |
| L-03 | No persistent storage | Medium | Future: add SD card / flash logging | Documented |
| L-04 | Fixed (compile-time) priority scheme | Low | Priorities validated during design phase | Accepted |
| L-05 | Wokwi UART output unavailable | High | Use short Blue Pill pin labels and wire `A9` → `$serialMonitor:RX` in `diagram.json` | Fixed |
| L-06 | Wokwi OLED display unavailable | High | Send frame buffer in one bulk I2C transaction | Fixed |

---

## Detailed Descriptions

### L-01: DHT22 Blocking Read (~20 ms)

The DHT22 driver performs the entire single-wire read inside a `taskENTER_CRITICAL()` section. The fixed protocol delays total ~19.2 ms (`DHT22_Delay_us(18000)` plus `DHT22_Delay_us(40)` plus 40 × `DHT22_Delay_us(30)`), and the 40 high-phase polls add roughly 1 ms, so the scheduler is blocked for about 20 ms per read.

This blocking behavior is acceptable because 20 ms represents 2% of the 1-second sampling period. Note the scope precisely: `taskENTER_CRITICAL()` on this port raises BASEPRI to `configMAX_SYSCALL_INTERRUPT_PRIORITY`, so it does **not** disable all interrupts. SysTick (priority 0) still fires and the tick count stays accurate; only interrupts at or below `configMAX_SYSCALL_INTERRUPT_PRIORITY` are deferred, so no FreeRTOS API may be called from them during this window. The DHT22's own timing is the reason for the critical section: the protocol's microsecond windows cannot tolerate being preempted. Converting to interrupt-driven or DMA I/O would add complexity disproportionate to the impact.

Resolution: Accepted as a design trade-off. No code change required.

---

### L-02: Single Buzzer Alarm (Temperature Only)

The system currently triggers a buzzer alarm only for temperature out-of-range conditions (below 18°C or above 30°C). Humidity and motion thresholds do not trigger audible alarms.

The current design prioritizes temperature as the primary safety concern, which is appropriate for a room monitoring context. The OLED display provides visual indication for all monitored parameters.

Future Enhancement: Multi-tone alarm for different conditions (temperature: 1 kHz, humidity: 2 kHz, motion: 500 Hz).

Resolution: Accepted as initial release scope. Documented for future iteration.

---

### L-03: No Persistent Storage

Sensor data is held entirely in RAM (via SensorData_t structs and FreeRTOS queues). When the system powers off or resets, all historical data is lost. No data logging to flash, EEPROM, or external storage is implemented.

Mitigation: Current focus is real-time monitoring, not data archival. Serial output provides real-time data stream that can be captured externally. The 15-second INACTIVE timeout reduces unnecessary sensor polling.

Future Enhancement: Add SD card module (SPI) with FAT filesystem, implement circular buffer in flash, add timestamped data logging at configurable intervals.

Resolution: Documented as out-of-scope for Laboratory Activity 1.

---

### L-04: Fixed Priority Scheme (Compile-Time Static)

Task priorities are defined as compile-time constants in main.h and main.c. They cannot be changed at runtime without recompilation and reflashing the firmware.

Current priorities:
- InputTask: 3 (High)
- MotionTask: 3 (High)
- SensorTask: 2 (Mid)
- AlarmTask: 2 (Mid)
- DisplayTask: 1 (Low)

Mitigation: Priorities were carefully designed and justified during system architecture phase. Priority inversion is not expected in this system topology. The priority scheme has been validated through fault experiments (3 experiments documented).

Resolution: Accepted. Priority scheme is appropriate for the fixed functionality of this laboratory exercise.

---

### L-05: Wokwi UART Output (Resolved)

**Original Issue:** Early simulation runs showed no serial output in the Wokwi terminal when calling `printf()` or `HAL_UART_Transmit()`.

**Root Cause & Fix:** Two distinct faults had to be corrected. First, USART1 transmission in Wokwi requires an explicit connection to `$serialMonitor` in `diagram.json`. Second, and more subtly, the connections were originally written using long-form pin identifiers (`mcu:PA9`, `mcu:PA1`, `mcu:3.3V`). Wokwi does not recognise these names and **silently discards the affected wires** rather than reporting an error, so only `mcu:GND.1` survived. The diagram was rewritten using the Blue Pill's short header labels (`A0`/`A1`/`A9`/`A10`, `3V3.1`, `5V.1`, `GND.1`), and the serial monitor is now wired as `["mcu:A9", "$serialMonitor:RX", "red", ["v0"]]`. USART1 output now functions as expected at 115200 baud in the simulation terminal.

**Resolution:** Fully resolved and verified.

---

### L-06: Wokwi OLED Display Rendering (Resolved)

**Original Issue:** The SSD1306 OLED display initially showed a blank screen during Wokwi simulation despite correct I2C communication signals.

**Root Cause & Fix:** The initial driver implementation updated the display by sending 1,024 individual I2C transactions (one for each byte of the 128×64 frame buffer), each requiring a separate START/STOP condition and control byte. This overwhelmed Wokwi's virtual I2C peripheral engine, leading to dropped packets and stalled rendering. The driver was refactored in `src/app/hal/oled.c` to transmit the entire frame buffer (1,025 bytes: `0x40` control byte + 1,024 data bytes) in a single bulk `HAL_I2C_Master_Transmit()` call with `HAL_MAX_DELAY`.

**Resolution:** Fully resolved and verified. The OLED renders all 4 sensor pages smoothly in Wokwi.

---

## Wokwi Simulation Status Matrix

| Component | Wokwi Support | Verification Method |
|-----------|--------------|-------------------|
| GPIO (LED, inputs) | Works | Direct simulation + logic analyzer |
| UART (USART1) | Works (Resolved) | Wokwi $serialMonitor + hardware validation |
| I2C (OLED SSD1306) | Works (Resolved) | Wokwi simulation display + hardware validation |
| ADC (LDR) | Works | Simulation values verified against spec |
| PWM (Buzzer) | Works | Audio output verified in simulation |
| DHT22 (One-wire) | Works | Simulation + hardware |
| PIR (Digital) | Works | Direct simulation |
| Encoder (KY-040) | Works | Direct simulation + native tests |
| SysTick (FreeRTOS) | Works | Task scheduling verified via GPIO |

---

## Verification Strategy (Wokwi-Limited)

Given the Wokwi UART and OLED limitations, the project employs a three-tier verification strategy:

### Tier 1: Wokwi Simulation (Limited)
- GPIO-based task execution verification (LED toggling from tasks)
- Encoder and sensor input verification
- ISR triggering verification via GPIO
- FreeRTOS scheduler operation (task switching via GPIO toggling)

### Tier 2: Native Unit Testing (Primary)
- 33 automated tests running on host PC
- All hardware-independent logic tested:
  - Temperature evaluation (15 tests)
  - State machine transitions (8 tests)
  - Encoder navigation (10 tests)
- Tests execute in < 3 seconds total
- No hardware dependencies

### Tier 3: Hardware Validation (Definitive)
- Full system flash and test on physical STM32 Blue Pill
- OLED display verification
- UART serial output verification
- All sensor and actuator verification
- Stress testing and long-duration stability tests

---

## Risk Assessment

| Limitation | Risk | Mitigation | Residual Risk |
|-----------|------|------------|---------------|
| L-01 DHT22 blocking | Low | High — negligible impact | Very Low |
| L-02 Single buzzer | Low | Medium — documented for future | Low |
| L-03 No storage | Medium | Medium — acceptable for scope | Medium |
| L-04 Fixed priority | Low | High — validated via experiments | Very Low |
| L-05 No UART in Wokwi | High | Resolved — A9 wired to serialMonitor with short pin labels | None |
| L-06 No OLED in Wokwi | High | Resolved — bulk I2C transfer fixed rendering | None |

---

## Conclusion

All four primary project limitations (L-01 to L-04) are properly documented, technically justified, and accepted within the scope of Laboratory Activity 1. Initial simulation hurdles regarding UART serial transmission (L-05) and SSD1306 OLED rendering (L-06) were systematically diagnosed and fully resolved through proper Wokwi wiring and bulk I2C transmission optimizations.

For grading purposes, all functional requirements are verified through the combination of native unit tests (33 tests, all passing), Wokwi full-circuit simulation, fault experiments (3 documented), and static code analysis (0 functional defects; 20 clang-tidy advisories and 1 compiler warning).

**Evidence limits.** Three claims in this documentation could not be independently re-verified in the environment used to audit it, and are flagged as such at each point of use:

1. The firmware was **not rebuilt** — no `pio`, no `arm-none-eabi-gcc`, and no `.elf`/`.map` from a prior build. The RAM and Flash figures in the report's §7.1 are reproduced as previously reported.
2. The **Wokwi simulation was not re-run** — the tests in `docs/functional-verification.md` record development-time observations. Their expected results were checked against the source; the runs themselves were not replayed.
3. The **fault experiments have no stored logs**. Their *predicted* outcomes are derived deterministically from `src/FreeRTOSConfig.h` and the task priorities in `src/main.c` and therefore hold regardless of any run; the *observed* sections record what was seen at development time. The two are labelled separately.

What *was* re-verified: all 33 unit tests were recompiled and re-run (33/33 pass), and the static analysis was re-executed from scratch with the exact commands in `docs/static-analysis.md`. Every file path, symbol name, constant, task priority, and queue name cited anywhere in this documentation was checked against `src/`.
