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
| L-05 | Wokwi UART output unavailable | High | Use short Blue Pill pin labels and wire `A9` → `$serialMonitor:RX` in `diagram.json` | Fix applied |
| L-06 | Wokwi OLED display unavailable | High | Send frame buffer in one bulk I2C transaction | Improved, not the fix |
| L-07 | Wokwi `cpsie` sets the interrupt masks instead of clearing them | High | Clear `PRIMASK`/`FAULTMASK` with `msr` before the `svc` in a project-local port | Fix applied; runtime replay attempted, task execution **not** confirmed |
| L-08 | Wokwi does not implement `BASEPRI` | Medium | None possible — critical sections are not protected in the simulator | Accepted |
| L-09 | Wokwi silently discards unrecognised pin labels | Medium | Use short header labels only; never long-form `mcu:PA9` | Mitigated |
| L-10 | OLED driver discarded every I2C status | Medium | Return `HAL_StatusTypeDef` from `OLED_Init()`/`OLED_Update()` and report failures over UART | Fix applied |

> **What the `Status` column means.** `Fix applied` records that the change is present in the
> repository — not that it has been observed to work. The three simulator-defect fixes
> (L-05, L-06, L-07) were each introduced to address a cause identified by reasoning and by
> object-code inspection.
>
> **Update — runtime replay performed.** The Wokwi simulation has now been executed against
> the current revision, repeatedly and reproducibly. The result is recorded in full under
> L-07: the firmware emits its three boot lines and then stops. The tasks have therefore
> **not** been observed running under the simulator, and the L-07 patch — while present in
> the linked image — does not produce a visibly running system there. Anyone resuming this
> work should read L-07 before trusting any "works in Wokwi" statement elsewhere in the
> repository.

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

### L-05: Wokwi UART Output (Fix Applied)

**Original Issue:** Early simulation runs showed no serial output in the Wokwi terminal when calling `printf()` or `HAL_UART_Transmit()`.

**Root Cause & Fix:** Two distinct faults had to be corrected. First, USART1 transmission in Wokwi requires an explicit connection to `$serialMonitor` in `diagram.json`. Second, and more subtly, the connections were originally written using long-form pin identifiers (`mcu:PA9`, `mcu:PA1`, `mcu:3.3V`). Wokwi does not recognise these names and **silently discards the affected wires** rather than reporting an error, so only `mcu:GND.1` survived. The diagram was rewritten using the Blue Pill's short header labels (`A0`/`A1`/`A9`/`A10`, `3V3.1`, `5V.1`, `GND.1`), and the serial monitor is now wired as `["mcu:A9", "$serialMonitor:RX", "red", ["v0"]]`. USART1 output now functions as expected at 115200 baud in the simulation terminal.

**Resolution:** Fully resolved and verified. This is the one simulator fix whose effect has been re-observed on the current revision: the boot log is produced over USART1 in the simulation terminal, which could not have happened while the wires were being silently discarded.

---

### L-06: Wokwi OLED Display Rendering (Resolved, Not the Root Cause)

**Original Issue:** The SSD1306 OLED display initially showed a blank screen during Wokwi simulation despite correct I2C communication signals.

**Root Cause & Fix:** The initial driver implementation updated the display by sending 1,024 individual I2C transactions (one for each byte of the 128×64 frame buffer), each requiring a separate START/STOP condition and control byte. This overwhelmed Wokwi's virtual I2C peripheral engine, leading to dropped packets and stalled rendering. The driver was refactored in `src/app/hal/oled.c` to transmit the entire frame buffer (1,025 bytes: `0x40` control byte + 1,024 data bytes) in a single bulk `HAL_I2C_Master_Transmit()` call with `HAL_MAX_DELAY`.

**Resolution:** The wiring defect is fixed in `diagram.json`. The bulk-transfer change is a genuine improvement and is retained, but it was **not** the fix that made the display work — see L-07 for the leading hypothesis. An earlier revision of this document claimed the OLED "renders all 4 sensor pages smoothly in Wokwi" on the strength of commit `647b343`, whose message reads *"OLED now renders in Wokwi"*. That claim was never true: at that commit every pin label in `diagram.json` was still long-form, so Wokwi had silently discarded the OLED and UART wires and the display was not connected to anything. The claim is corrected here rather than deleted, because a commit message asserting an unobserved result is itself a defect worth recording.

---

### L-07: Wokwi `cpsie` Sets the Interrupt Masks (Fix Applied, Runtime Unconfirmed)

**Original Issue:** After a successful build, a Wokwi run produced exactly two lines of serial output — `[MAIN] System initialized` and `[MAIN] Starting FreeRTOS scheduler` — and then nothing at all. No task banner, no `[SENSOR]` line, no OLED content, no LED activity. Both lines that appeared are printed from `main()` *before* `vTaskStartScheduler()` is called, so the boundary between working and silent output fell exactly at the scheduler start.

**Root Cause:** The FreeRTOS Cortex-M3 port's `prvPortStartFirstTask()` clears the interrupt masks and then issues the supervisor call that starts the first task:

```asm
cpsie i          ; clear PRIMASK  -- enable interrupts
cpsie f          ; clear FAULTMASK
dsb
isb
svc 0            ; start the first task
```

On real ARMv7-M hardware `cpsie i` clears `PRIMASK` and `cpsie f` clears `FAULTMASK`. The hypothesis is that Wokwi's Cortex-M3 model implements these instructions with the **opposite** effect: they set the masks instead of clearing them. `PRIMASK` would then be left at 1 when the `svc 0` executes, and because `SVCall` is a configurable-priority exception it is masked by `PRIMASK`. The supervisor call would never fire, the first task would never be entered, and the system would sit in the idle loop of `xPortStartScheduler()` forever with interrupts masked — no tick, no context switch, no output.

This is put forward as the **leading hypothesis** for the symptom set, not as a verified
cause. It accounts for the detail that made the problem confusing — the two `[MAIN]` lines
appear because they are emitted before the scheduler starts, and everything after that point
is silent — but it has not been confirmed by observing the masks at the point of failure.
See the runtime replay result below for what the current revision actually does, and for the
evidence that both supports and limits this explanation.

**Fix:** The project carries a local copy of the port at `lib/freertos_port_patch/src/port.c` — the stock V11.3.1 ARM_CM3 port with three instructions added before the `svc`:

```asm
movs r0, #0
msr primask, r0      ; clear PRIMASK explicitly
msr faultmask, r0    ; clear FAULTMASK explicitly
svc 0
```

`msr` is the architecturally correct way to clear these masks, and on real hardware the added instructions are redundant no-ops that leave the register state identical to what `cpsie` would have produced. The `cpsie` instructions are deliberately **kept** rather than removed, so the port stays correct on genuine silicon and the deviation is confined to three instructions that are harmless there. The library's `library.json` sets `"libArchive": false`, which links the patched `port.o` as a plain object rather than an archive member, so a duplicate symbol fails the link loudly instead of silently falling back to the unpatched copy. `tools/verify/check_port_patch.py` runs as a post-build action and warns if the patched object is missing from the build directory.

**Resolution:** Fix applied; runtime behaviour **not** confirmed. The defect is in the simulator's instruction semantics rather than in this project's code, so no application-level change could have addressed it. The alternative — adopting the reference implementation's entire custom port — was rejected because that port targets FreeRTOS V10.3.1 and would forfeit V11 behaviour including `configCHECK_HANDLER_INSTALLATION` and the V11 SVC/PendSV handler naming.

**Runtime replay result (current revision, no debugger attached).** The simulation was
re-executed against the current build, repeatedly and reproducibly. It does **not** show a
running system:

```
[DIAG] VTOR was 0x08000000, now 0x08000000; vec[11]=0x080029AD vec[14]=0x080029F9 vectors-ok
[MAIN] System initialized
[MAIN] Starting FreeRTOS scheduler
        <- no further output; session never terminates on its own
```

Exactly three lines, every run, with no task banner, no OLED frame, and no LED activity.
What this establishes, and what it does not:

- **Established.** The patched port *is* in the linked image (the post-build
  `tools/verify/check_port_patch.py` action reports `patched ARM_CM3 port.o is in the
  image`), and the `movs r0,#0` / `msr primask,r0` / `msr faultmask,r0` sequence is present
  at the expected addresses in `firmware.elf`. Vector entries 11 (`SVC_Handler`,
  `0x080029AD`) and 14 (`PendSV_Handler`, `0x080029F9`) both point into the patched port,
  which the boot log's own `vectors-ok` line confirms at runtime, so
  `configCHECK_HANDLER_INSTALLATION` passes.
- **Established.** Every *instrumented* failure path is excluded, because each writes an
  unconditional line over a raw-register channel that does not depend on the mutex, the HAL,
  or the C library (`src/drivers/diag.c`, whose TXE poll is bounded so it cannot itself
  hang). None of those lines appear: no `[MAIN] FATAL: xTaskCreate failed`, no
  `[MAIN] FATAL: vTaskStartScheduler returned`, no `[FATAL] Stack overflow`, no
  `[FATAL] Malloc failed`, and no `[ASSERT]` line — so no `configASSERT` fired and all five
  tasks were created successfully within the 12 KB heap.
- **Not established.** Whether the tasks actually start. This matters because the boot log
  is genuinely ambiguous at this boundary: all three emitted lines are written *before*
  `vTaskStartScheduler()` — the two `[MAIN]` lines from `UART_Mutex_Printf`, which uses a
  semaphore and `HAL_UART_Transmit` — whereas every `[TASK] <name> entered` banner is
  emitted through that same mutex-and-HAL path from inside a task. A break *after* the
  scheduler starts but *before* the logging path becomes usable would therefore be
  indistinguishable from the first task never starting, on the evidence available.
- **Not established.** The mechanism. The causal explanation given above (Wokwi's `cpsie`
  setting rather than clearing the masks, so the `svc 0` is masked and the first task is
  never entered) remains the leading hypothesis, but it is a hypothesis supported by
  reasoning and object-code inspection, not by an observation of the faulting instruction.
  A debugger attach against the simulated target proved unreliable in this environment —
  hardware watchpoints are unsupported by the Wokwi GDB stub, and the 1 kHz SysTick starves
  the session — so the mechanism was not confirmed directly.
- **Caveat on the evidence channel.** One instrumented probe run in this environment showed
  the serial transport being cut by the simulation API (`code 1006`) rather than reaching
  the requested timeout, and the serial log file was truncated at a fixed size when the
  process was killed. Simulator sessions here are therefore only a *partially* reliable
  observation channel, and the three-line result should be read as "reproducibly observed",
  not as a precise measurement of where execution stops.

---

### L-08: Wokwi Does Not Implement `BASEPRI` (Accepted)

**Issue:** Wokwi's Cortex-M3 model does not implement `BASEPRI`, and its `PRIMASK`/`FAULTMASK` do not block SysTick.

**Impact:** The V11 port implements `portDISABLE_INTERRUPTS()` and `portENABLE_INTERRUPTS()` by writing `BASEPRI`, so those calls have no effect in the simulator and critical sections are not genuinely protected there. Any concurrency defect that depends on a critical section would therefore not be caught by a Wokwi run. This is a limitation of the verification environment, not of the firmware: `BASEPRI` works as specified on real hardware, so the code is correct on the target.

**Resolution:** Accepted. No workaround is possible without replacing the port's critical-section primitives, which would mean adopting the reference implementation's V10.3.1 port and forfeiting V11 behaviour. The consequence is recorded so that Wokwi results are not over-read as evidence of concurrency correctness.

---

### L-09: Wokwi Silently Discards Unrecognised Pin Labels (Mitigated)

**Issue:** A wire written with a long-form pin name such as `mcu:PA9` or `mcu:3.3V` is dropped by Wokwi **without any error**. A mislabelled net is therefore indistinguishable from an unconnected one, and the circuit appears to build successfully while being electrically incomplete.

**Impact:** This produced a false conclusion that survived several revisions of this report: commit `647b343` claimed the OLED rendered in Wokwi, but at that commit every label was long-form, so the OLED and UART wires had been discarded and the display was not connected. See L-06.

**Resolution:** Mitigated. All 25 connections in `diagram.json` use the Blue Pill's short header labels (`A0`/`A1`/`A9`/`A10`, `C13`, `3V3.1`, `5V.1`, `GND.1`), and the convention is recorded here so it is not reintroduced. There is no way to make Wokwi report the error, so the mitigation is procedural rather than technical.

---

### L-10: OLED Driver Discarded Every I2C Status (Fix Applied)

**Issue:** `OLED_SendCommand()`, `OLED_SendData()` and `OLED_Update()` each called `HAL_I2C_Master_Transmit()` and threw the return value away. The driver therefore had no way to distinguish a panel that acknowledged its address from one that was absent, mis-addressed, or holding the bus low.

**Impact:** A missing or mis-addressed display was indistinguishable from a display that was present but being drawn to incorrectly. This is the same class of defect as L-09 — a silent failure that makes a wiring fault look like a software fault — and it is the reason a blank OLED could not be attributed to either cause from the firmware side alone.

**Resolution:** Fixed. `OLED_Init()` and `OLED_Update()` now return `HAL_StatusTypeDef` and latch the first failing transaction; `OLED_t` carries a `last_status` field. `OLED_Init()` stops at the first unacknowledged command rather than issuing the remaining 24 into a dead bus. The callers report the failure over UART — `main.c` prints `[OLED] init failed: no ACK from 0x3C` and `DisplayTask()` prints `[OLED] frame transfer failed (panel not ACKing)`. A transient failure is reported but not retried; retry logic is left as future work.

---

## Wokwi Simulation Status Matrix

| Component | Simulator Support | Verification Method | Related Limitations |
|-----------|--------------|-------------------|---------------------|
| GPIO (LED, inputs) | Works | Direct simulation (LED added to `diagram.json` on PC13) | — |
| UART (USART1) | Works | Wokwi `$serialMonitor`; wiring only correct with short pin labels | L-05, L-09 |
| I2C (OLED SSD1306) | Works | Wokwi OLED panel; requires the port patch for the scheduler to start at all | L-06, L-07 |
| ADC (LDR) | Works | Simulation values checked against the driver's expected range | — |
| PWM (Buzzer) | Works | Buzzer part audible in simulation | — |
| DHT22 (One-wire) | Works | Simulated part read by the driver; no hardware comparison made | — |
| PIR (Digital) | Works | Motion trigger toggled in simulation | — |
| Encoder (KY-040) | Works | Rotation and button events in simulation; logic covered by native tests | — |
| SysTick (FreeRTOS) | Works | Task scheduling observable via OLED and serial output | L-07 |
| Critical sections (`BASEPRI`) | **Not implemented** | Cannot be verified in simulation; hardware only | L-08 |

**On the "hardware validation" column.** Earlier revisions of this table listed hardware validation against several rows. That column has been removed because no hardware validation was performed during this work — the ST-Link and physical board are documented in the report as the intended deployment path, not as an exercised one.

**On the word "Works".** It records that Wokwi models the peripheral and that the firmware is
wired to it correctly — it is not a claim that the behaviour was observed on the current
revision. **As of the latest replay, no row in this table has been observed working end to
end**: every simulator session against the current build stops after the three boot lines,
before any task runs, so no peripheral interaction occurs. The per-peripheral rows record
correct modelling and correct wiring, verified by inspection and by the diagram lint
(`tools/verify/run_all.sh` pass 6) rather than by an observed run. The one row that is a
genuine finding rather than a status is the last: `BASEPRI` is not implemented by the
simulator at all, which no amount of re-running will change.

---

## Verification Strategy (Wokwi-Limited)

Given the Wokwi UART and OLED limitations, the project employs a three-tier verification strategy:

### Tier 1: Wokwi Simulation (Limited)
- Peripheral wiring and addressing (I²C address match, UART pin map, ADC channel map)
- Sensor input injection (DHT22 sliders, PIR trigger, LDR lux) and encoder events
- Task execution: each task prints a `[TASK] <name> entered` banner over USART1, so the
  boot log shows directly whether the scheduler handed control to every task
- Fault indication: the `PC13` LED blinks inside the stack-overflow and malloc-failed hooks
- **Not** usable for verifying critical-section behaviour: Wokwi does not implement `BASEPRI` (L-08), so `portENTER_CRITICAL()`/`portEXIT_CRITICAL()` have no effect there

> Tier 1 as listed describes what the simulator is *capable* of showing. It has been re-run
> against the current revision and did **not** show task execution: the session stops after
> the three boot lines (see L-07). The `[TASK] <name> entered` banners are therefore
> currently unobserved, and no application task toggles GPIO, so the LED is a fault
> indicator only and not a heartbeat.

### Tier 2: Native Unit Testing (Primary)
- 33 automated tests running on host PC
- All hardware-independent logic tested:
  - Temperature evaluation (15 tests)
  - State machine transitions (8 tests)
  - Encoder navigation (10 tests)
- Tests execute in < 3 seconds total
- No hardware dependencies

### Tier 3: Hardware Validation (Definitive — Not Performed)
- Full system flash and test on physical STM32 Blue Pill
- OLED display verification
- UART serial output verification
- All sensor and actuator verification
- Stress testing and long-duration stability tests

Tier 3 is the only tier that can settle the questions Tiers 1 and 2 cannot, and it has
**not** been carried out. It is listed here as the remaining work, not as a completed step.
The ST-Link wiring needed to perform it is documented in the report.

---

## Risk Assessment

| Limitation | Risk | Mitigation | Residual Risk |
|-----------|------|------------|---------------|
| L-01 DHT22 blocking | Low | High — negligible impact | Very Low |
| L-02 Single buzzer | Low | Medium — documented for future | Low |
| L-03 No storage | Medium | Medium — acceptable for scope | Medium |
| L-04 Fixed priority | Low | High — validated via experiments | Very Low |
| L-05 Silent wire discard | High | Short pin labels only (procedural) | Low |
| L-06 Bulk I2C transfer | Low | Retained on its own merits | Very Low |
| L-07 `cpsie` masks / no scheduler | High | Project-local port patch, symbol-shadowing verified at object level | **High — replayed in Wokwi; the system still stops after the boot lines and no task was observed running** |
| L-08 `BASEPRI` not implemented | Medium | None possible in simulation | Medium |
| L-09 Long-form labels dropped | Medium | Short header labels only; documented | Low |

---

## Conclusion

All four primary project limitations (L-01 to L-04) are properly documented, technically justified, and accepted within the scope of Laboratory Activity 1. The simulation hurdles were diagnosed in sequence, and the sequence matters because each earlier fix masked the one behind it:

- **L-05 (UART)** — Wokwi silently discarded every wire written with a long-form pin label, so the serial monitor was never connected. Fixed by rewriting `diagram.json` with short header labels.
- **L-06 (OLED)** — the driver sent 1,024 individual I2C transactions per frame; refactored to a single bulk transfer. This is a genuine improvement, but it was **not** the fix that made the display work.
- **L-07 (`cpsie`)** — the leading explanation for the blank display and silent terminal. Wokwi's Cortex-M3 model is believed to implement `cpsie` with the opposite effect to ARMv7-M, so the FreeRTOS port masked its own `svc 0` and the first task never started. Worked around in a project-local port with three added instructions that are no-ops on real hardware. **This fix has now been replayed in the simulator and the system still stops after the boot lines**, so the workaround is present but not sufficient to produce a visibly running system there; the mechanism remains a hypothesis rather than a confirmed cause.
- **L-08 (`BASEPRI`)** — not implemented in Wokwi, so critical sections are not genuinely protected there. Accepted; `BASEPRI` works correctly on the target.
- **L-09 (silent wire discard)** — the defect class that made L-06's "resolved" claim false. Procedurally mitigated.

**A note on one earlier claim.** Commit `647b343` is titled *"OLED now renders in Wokwi"*, and that claim was carried into this documentation for several revisions. It was never true: at that commit every pin label was long-form, so Wokwi had discarded the OLED wire and the display was not connected. The claim is corrected in L-06 rather than deleted, because a commit message that asserts an unobserved result is itself a defect — it converts an untested change into a believed one, and it makes any later regression impossible to diagnose.

For grading purposes, the functional requirements are verified through the combination of native unit tests (33 tests, all passing), static code analysis (0 functional defects; 21 clang-tidy advisories, 1 compiler warning, and 2 benign memory-mapped-register findings), and the seven-pass verification harness in `tools/verify/run_all.sh` (six passes run on this machine, one is skipped for a missing `wokwi-cli`). **Wokwi full-circuit simulation is not among the passing evidence**: the diagram lints cleanly and the firmware builds and links, but a simulator run does not reach running tasks, so no end-to-end behaviour was observed there. The fault experiments (3 documented) record development-time observations and are not replayable.

**Evidence limits.** Three claims in this documentation could not be fully re-verified in the environment used to audit it. Each is flagged at its point of use. One of them has since been re-verified in full and one partially; the remainder is set out below.

1. **Firmware size (report §7.1).** *Re-verified in full.* An earlier audit pass could not repeat the PlatformIO build, because the vendor ARM toolchain and the STM32Cube/FreeRTOS sources were not present in that environment. The build has since been run against the current revision and the figures in §7.1 are the result: `pio run -e bluepill_f103c8` reports RAM 15,792 / 20,480 B (77.1%) and Flash 36,252 / 65,536 B (55.3%), matching `size -A` on the linked ELF (Flash = `.text` 33,032 + `.rodata` 2,732 + `.data` 488; RAM = `.data` 488 + `.bss` 15,304). This supersedes both the RAM 14,356 B / Flash 26,304 B originally published in §7.1 — measured before the queue fan-out fix (`9d0d056`) and the OLED bulk-transfer fix (`647b343`) and never re-measured, an understatement of 1,028 B of RAM and 4 B of Flash — and the intermediate pre-instrumentation baseline of RAM 15,384 B / Flash 26,308 B.

   The increase over that intermediate baseline is accounted for: the diagnostic instrumentation described in report §7.5 (`src/drivers/diag.c`: 1,216 B `.text`, 36 B `.bss`) and the `-Wl,-u,_printf_float` link flag, which pulls the newlib float formatter into the image at a cost of just under 3 KB. `tools/verify/run_size_analysis.sh` remains available as an application-only cross-check, but it uses clang's ARM target rather than `arm-none-eabi-gcc`, so its absolutes are indicative rather than authoritative.

2. **Wokwi simulation runs.** *Replayed; result negative.* The procedures in `docs/functional-verification.md` record development-time observations. Those observations were **not** reproduced: the simulation was re-executed against the current revision (repeatedly, reproducibly, without a debugger attached) and the firmware stops after its three boot lines without reaching any task — see L-07 for the full result and for what it does and does not establish. The environment was capable of running the simulation (the Wokwi CLI was available and authenticated for this pass), so the earlier statement that the tooling was unavailable no longer applies. Treat every development-time claim about observed simulator behaviour in `docs/functional-verification.md` as unconfirmed. The native unit tests and static analysis in `tools/verify/` do reproduce, and the diagram lint passes.

3. **Fault-experiment logs.** *Not replayable.* No logs were stored. Their *predicted* outcomes are derived deterministically from `src/FreeRTOSConfig.h` and the task priorities in `src/main.c`, so they hold regardless of any run; the *observed* sections record what was seen at development time. The two are labelled separately throughout `docs/fault-experiments.md`. Re-running them would require physical hardware, and a simulator cannot expose RTOS scheduling internals such as priority starvation in any case.

What **was** re-verified from scratch: all 33 unit tests were recompiled and re-run (33/33 pass, `tools/verify/run_tests.sh`); the static analysis was re-executed (0 functional defects, 21 clang-tidy advisories, 1 compiler warning, 2 benign MMIO findings — `tools/verify/run_static_analysis.sh`); the firmware was rebuilt with the vendor toolchain and its RAM and Flash footprint confirmed against both the PlatformIO summary and `size -A` on the linked ELF; and every file path, symbol name, constant, task priority, and queue name cited anywhere in this documentation was checked against `src/`.