# Known Limitations — BCA182 Room Monitoring System

**Date:** September 2026  
**Project:** BCA182 Laboratory Activity 1 — Room Monitoring System  
**Platform:** STM32 Blue Pill (STM32F103C8T6) + FreeRTOS + Wokwi Simulation

---

## Summary Table

| # | Limitation | Severity | Workaround | Status |
|---|-----------|----------|------------|--------|
| L-01 | DHT22 blocking read (~5 ms) | Low | Acceptable given 1 s sampling period (0.5% CPU) | Accepted |
| L-02 | Single buzzer (temperature only) | Low | Future: add humidity/motion alarms | Accepted |
| L-03 | No persistent storage | Medium | Future: add SD card / flash logging | Documented |
| L-04 | Fixed (compile-time) priority scheme | Low | Priorities validated during design phase | Accepted |
| L-05 | Wokwi UART output unavailable | High | Use short Blue Pill pin labels and wire `A9` → `$serialMonitor:RX` in `diagram.json` | Fix applied |
| L-06 | Wokwi OLED display unavailable | High | Send frame buffer in one bulk I2C transaction | Improved, not the fix |
| L-07 | SysTick gated permanently by a pre-scheduler critical section | High | Defer `vPortEnterCritical()`/`vPortExitCritical()` while `xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED` | **Resolved — verified running in Renode** |
| L-08 | Wokwi does not implement `BASEPRI` | Medium | None possible — critical sections are not protected in the simulator | Accepted |
| L-09 | Wokwi silently discards unrecognised pin labels | Medium | Use short header labels only; never long-form `mcu:PA9` | Mitigated |
| L-10 | OLED driver discarded every I2C status | Medium | Return `HAL_StatusTypeDef` from `OLED_Init()`/`OLED_Update()` and report failures over UART | Fix applied |
| L-11 | UART mutex used before it was created | High | Create the mutex before the first code path that can report a fault | Fix applied |
| L-12 | OLED transactions used `HAL_MAX_DELAY` | High | Bound every transaction with `OLED_I2C_TIMEOUT_MS` | Fix applied |
| L-13 | `portYIELD_FROM_ISR` compiled against the stock header | High | Call `vPortYieldFromISR()` through an `extern` declaration | Fix applied |

> **What the `Status` column means.** `Fix applied` records that the change is present in the
> repository — not that it has been observed to work. `Resolved` records that the fix has
> additionally been *reproduced working* under a runtime harness. Only L-07 carries that
> status, on the strength of a Renode replay in which all five tasks were observed running.
>
> **Update — the hang is fixed and the fix has been replayed.** The firmware previously
> printed three boot lines and stopped. The cause was found (L-07), fixed, and the fix
> re-verified in Renode: `uwTick` now advances, SysTick is never left gated, and the system
> reaches `[MAIN] Starting FreeRTOS scheduler` and enters all five tasks. The earlier
> `cpsie`/`PRIMASK` explanation recorded in previous revisions of this document was **wrong**
> and has been withdrawn. See L-07 for the evidence, and L-11/L-12/L-13 for three further
> real defects found by the same replay.

---

## Detailed Descriptions

### L-01: DHT22 Blocking Read (~5 ms)

The DHT22 driver performs the single-wire handshake and bit capture inside a `taskENTER_CRITICAL()` section. The 2 ms host start pulse is **not** inside it: it is produced with `vTaskDelay()` before the critical section is entered, so the scheduler is only held off for the reply itself — the 80/80/50 µs handshake plus 40 bits of 50 µs low and 26-70 µs high, roughly 4-5 ms.

This blocking behavior is acceptable because 5 ms represents 0.5% of the 1-second sampling period. Note the scope precisely: `taskENTER_CRITICAL()` on this port raises BASEPRI to `configMAX_SYSCALL_INTERRUPT_PRIORITY`, so it does **not** disable all interrupts. SysTick (priority 0) still fires and the tick count stays accurate; only interrupts at or below `configMAX_SYSCALL_INTERRUPT_PRIORITY` are deferred, so no FreeRTOS API may be called from them during this window. The DHT22's own timing is the reason for the critical section: the protocol's microsecond windows cannot tolerate being preempted. Converting to interrupt-driven or DMA I/O would add complexity disproportionate to the impact.

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

**Root Cause & Fix:** The initial driver implementation updated the display by sending 1,024 individual I2C transactions (one for each byte of the 128×64 frame buffer), each requiring a separate START/STOP condition and control byte. This overwhelmed Wokwi's virtual I2C peripheral engine, leading to dropped packets and stalled rendering. The driver was refactored in `src/app/hal/oled.c` to transmit the entire frame buffer (1,025 bytes: `0x40` control byte + 1,024 data bytes) in a single bulk `HAL_I2C_Master_Transmit()` call. (That call originally used `HAL_MAX_DELAY`; the timeout is now bounded — see L-12.)

**Resolution:** The wiring defect is fixed in `diagram.json`. The bulk-transfer change is a genuine improvement and is retained, but it was **not** the fix that made the display work — see L-07 for the actual root cause. An earlier revision of this document claimed the OLED "renders all 4 sensor pages smoothly in Wokwi" on the strength of commit `647b343`, whose message reads *"OLED now renders in Wokwi"*. That claim was never true: at that commit every pin label in `diagram.json` was still long-form, so Wokwi had silently discarded the OLED and UART wires and the display was not connected to anything. The claim is corrected here rather than deleted, because a commit message asserting an unobserved result is itself a defect worth recording.

---

### L-07: SysTick Gated Permanently by a Pre-Scheduler Critical Section (Resolved)

**Symptom:** The firmware printed three serial lines and then went permanently silent — no
task banner, no `[SENSOR]` line, no OLED activity:

```
[DIAG] VTOR was 0x08000000, now 0x08000000; ... vectors-ok
[MAIN] System initialized
[MAIN] Starting FreeRTOS scheduler
        <- nothing further; the session never terminated on its own
```

**Root cause (proven).** The port disables the kernel tick with the simulation-safe
mechanism of clearing `SysTick->CTRL.TICKINT` rather than by using `BASEPRI`, which Wokwi
does not implement. That gate is taken in `vPortEnterCritical()` and released in
`vPortExitCritical()` **only when the nesting counter reaches exactly 0**:

```c
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;   /* "kernel not started" sentinel */

void vPortEnterCritical( void ) { vPortGateTick(); uxCriticalNesting++; }
void vPortExitCritical( void )  { uxCriticalNesting--; if( uxCriticalNesting == 0 ) vPortUngateTick(); }
```

`uxCriticalNesting` starts at `0xaaaaaaaa`, not `0`, to mark "the kernel does not exist
yet". Taken against that sentinel the counter walks `0xAAAAAAAA → 0xAAAAAAAB → 0xAAAAAAAA`
and **never reaches 0**, so the gate is opened on the first critical section and never
lifted. Only `xPortStartScheduler()` writes `uxCriticalNesting = 0`, and the firmware hangs
beforeever reaching it.

The trigger is this application's use of FreeRTOS before the scheduler exists. `main()`
calls `UART_Mutex_Init()` (→ `xSemaphoreCreateMutex()` → `xQueueGenericCreate()` →
`xQueueGenericReset()`) and five `xTaskCreate()` calls, all **before**
`vTaskStartScheduler()`. Those kernel functions execute `taskENTER_CRITICAL()` in
`queue.c`/`tasks.c`, which resolve the project's patched `portmacro.h` — so the very first
FreeRTOS object created permanently gates the tick. `OLED_Init()` then calls
`HAL_Delay(100)`, which waits on `HAL_GetTick()`, which needs that tick. It spins forever.

**Evidence (Renode 1.17.0, `stm32f103.repl`).** Sampling the CPU and memory during the
hang:

| Address | Symbol | Observed | Expected if healthy |
|---|---|---|---|
| `0x20000008` | `uxCriticalNesting` | `0xAAAAAAAA` in all 12 samples | `0` once scheduling |
| `0x20003DAC` | `uwTick` | frozen at `0x00000003` | advancing at 1 kHz |
| `0xE000E010` | `SysTick->CTRL` | `0x00010005` (TICKINT clear) | `0x00000007` |
| — | PC | `0x8001124`–`0x800114C` (`HAL_GetTick`/`HAL_Delay` wait loop) | anywhere else |

A 4 µs-resolution trace pinned the moment of failure between 128 µs and 428 µs of
simulated time: `CTRL` reads `0x00000007` at 128 µs (PC `0x800109C`, `HAL_InitTick` arming
SysTick) and `0x00000005` at 428 µs (PC `0x8001144`, already inside the `HAL_Delay` wait
loop) — i.e. the gate closed while `UART_Mutex_Init()` was creating the mutex, exactly as
the analysis predicts.

**Why this was hard to see.** Renode's NVIC models SysTick itself, with a fixed
`systickFrequency: 72000000`, independently of the RCC. The stall therefore could not be
explained away as a missing-clock-model artefact, but neither did it point at the critical
section until the nesting counter was sampled directly.

**Why the reference implementation is unaffected.** Its `vPortEnterCritical()` and
`vPortExitCritical()` are **identical** — the defect is latent there too. It is only
*accidentally* immune: its `main.cpp` creates its mutex and sensor queue and then calls
`vTaskStartScheduler()` with no `HAL_Delay` and no other HAL-tick dependency in between
(there is no `HAL_Delay` anywhere in its `src/`). Nothing that needs the tick runs while
the tick is gated, so the permanent gate costs it nothing. Our `OLED_Init()` does need the
tick before the scheduler, so the same latent defect is fatal here.

**Fix (applied).** Both critical-section primitives now defer to the scheduler state before
touching the gate:

```c
void vPortEnterCritical( void )
{
    if( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED )
    {
        return;
    }
    vPortGateTick();
    uxCriticalNesting++;
}
```

with the mirrored early return in `vPortExitCritical()`. This is safe because before the
scheduler starts there is exactly one execution context (`main()`), no task exists to
observe a kernel structure concurrently, and `SysTick_Handler()` already routes
`taskSCHEDULER_NOT_STARTED` to `vApplicationTickHook()` only (`HAL_IncTick()`), never
touching a kernel list — so there is no race for the gate to protect against. The two
early returns are symmetrical, so `uxCriticalNesting` stays at the `0xaaaaaaaa` sentinel,
which is exactly its documented meaning. `xTaskGetSchedulerState()` compiles to a plain
global read at `configNUMBER_OF_CORES == 1`, so it is safe to call from inside the
primitive.

**Re-verification (Renode, post-fix).** The same register sampling and serial capture now
show a running system:

| Address | Symbol | Observed after fix |
|---|---|---|
| `0x20000008` | `uxCriticalNesting` | `0xAAAAAAAA` pre-scheduler, then `0x00000000` |
| `0x20003DAC` | `uwTick` | advancing, `0x00000023 → 0x00000382` over 88 ms |
| `0xE000E010` | `SysTick->CTRL` | stays `0x00000007` (never gated) |
| — | PC | visibly in `prvIdleTask` / `prvCheckTasksWaitingTermination` |

and the serial port emits, in order:

```
[DIAG] VTOR was 0x08000000, now 0x08000000; vec[11]=0x08002B99 vec[14]=0x0800815D vectors-ok
[OLED] init failed: no ACK from 0x3C
[MAIN] System initialized
[MAIN] Starting FreeRTOS scheduler
[TASK] InputTask entered
[TASK] MotionTask entered
[TASK] AlarmTask entered
[TASK] SensorTask entered
[TASK] DisplayTask entered
[OLED] frame transfer failed (panel not ACKing)
[SENSOR] DHT22 read error
[SENSOR] LDR read error
[SENSOR] T=nanC H=nan% L=0 M=0
```

All five tasks are entered and the sensor task repeats, which demonstrates preemptive
multitasking: the tick, the context switch and the task bodies all execute. The
`[OLED]`/`[SENSOR]` error lines are correct behaviour for a Renode run, which models
neither the SSD1306 nor the DHT22/LDR; they are the diagnostic paths added under L-10
proving they now work.

**Correction to earlier revisions of this document.** L-07 previously recorded the
`cpsie` mask hypothesis as the leading explanation and stated that runtime behaviour was
unconfirmed. That hypothesis was **wrong** and has been withdrawn: the observed cause is
the critical-section/SysTick gate described above, confirmed by direct register
observation. The `cpsie` workaround in `prvPortStartFirstTask()` (three redundant `msr`
instructions after the `cpsie` pair) is retained because it is harmless on real silicon,
but it was never the fix.

**Status:** Resolved. Verified end-to-end in Renode.

---

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

### L-11: UART Mutex Used Before It Was Created (Fix Applied)

**Issue:** `main()` called `UART_Mutex_Printf(&uart_mutex, "[OLED] init failed: ...")` **before**
`UART_Mutex_Init(&uart_mutex, &huart1)` ran. `UART_Mutex_Printf()` takes
`uart_mutex->mutex`, which is still `NULL` at that point because `xSemaphoreCreateMutex()`
has not been called yet, so the kernel takes `xQueueSemaphoreTake(NULL)`.

**Impact:** `queue.c` guards that call with `configASSERT( ( pxQueue ) );` (line 1673 in
FreeRTOS V11.3.1), so instead of printing the message the firmware hit
`[ASSERT] Failed at .pio/libdeps/bluepill_f103c8/PlatformIO-FreeRTOS/FreeRTOS-Kernel/queue.c:1673`
and stopped. The diagnostic intended to explain a display fault instead became the fault.

**Why it was invisible.** On Wokwi the SSD1306 is present and acknowledges its address, so
`OLED_Init()` succeeds and the failure branch never executes. The bug only triggers when
the panel is absent — which is exactly what a Renode run reproduces, and it was found there.

**Resolution:** Fixed. `UART_Mutex_Init()` now runs immediately before `DHT22_Init()`, i.e.
before any code path that can report a fault, with a comment explaining the ordering
requirement.

---

### L-12: OLED Transactions Used `HAL_MAX_DELAY` (Fix Applied)

**Issue:** `OLED_SendCommand()` and the bulk transfer in `OLED_Update()` passed
`HAL_MAX_DELAY` to `HAL_I2C_Master_Transmit()`.

**Impact:** A panel that never acknowledges — absent, unpowered, or holding the bus low —
blocks the caller forever. Because `OLED_Init()` runs **before** `vTaskStartScheduler()`,
the whole application wedges during initialisation with no diagnostic output at all. This
also made the `[OLED] init failed` message added under L-10 unreachable: the code could
never get far enough to print it.

**Resolution:** Fixed. Both call sites now use `OLED_I2C_TIMEOUT_MS` (50 ms), which is
roughly 50× the worst-case transfer time at 100 kHz, so a healthy panel never reaches it.
`OLED_Update()` additionally returns early once the address-window commands fail, so a dead
panel costs one timeout rather than six. A dead panel is now reported and survived rather
than fatal.

---

### L-13: `portYIELD_FROM_ISR` Compiled Against the Stock Header (Fix Applied)

**Issue:** `main.c` compiles with the FreeRTOS library's (stock) `portmacro.h` ahead of the
project `include/` — the include order is per-translation-unit and PlatformIO puts library
directories first. The stock `portYIELD_FROM_ISR()` therefore became the real definition in
`HAL_GPIO_EXTI_Callback`, which stores `PENDSVSET` to the ICSR at `0xE000ED04`.

**Impact:** This port deliberately does **not** service PendSV — that is the whole point of
its design — and no strong `PendSV_Handler` is defined anywhere. The weak alias in the
startup file resolves to `Default_Handler`, so any external-interrupt yield would vector
into a permanent infinite loop. The fault was easy to miss because it only fires on an EXTI
edge, and the object-code evidence was subtle: `vPortYieldFromISR` was absent from the
symbol table entirely, having been dead-code-eliminated because nothing referenced it.

**Resolution:** Fixed. `main.c` declares `extern void vPortYieldFromISR(void);` and calls it
directly — the only way to bypass the stock header — replacing the three
`portYIELD_FROM_ISR()` calls in `HAL_GPIO_EXTI_Callback` with a single trailing
`if (xHigherPriorityTaskWoken != pdFALSE) { vPortYieldFromISR(); }`. Verified by
disassembly: the image now contains zero `0xE000ED04` stores (previously one) and
`vPortYieldFromISR` is present in the symbol table. `tools/verify/check_port_patch.py` now
asserts both of these conditions on every build.

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
| SysTick (FreeRTOS) | Works | Verified running under Renode: tick advances, all five tasks entered | L-07 |
| Critical sections (`BASEPRI`) | **Not implemented** | Cannot be verified in simulation; hardware only | L-08 |

**On the "hardware validation" column.** Earlier revisions of this table listed hardware validation against several rows. That column has been removed because no hardware validation was performed during this work — the ST-Link and physical board are documented in the report as the intended deployment path, not as an exercised one.

**On the word "Works".** It records that Wokwi models the peripheral and that the firmware is
wired to it correctly. The scheduler behaviour was additionally reproduced end-to-end under
Renode 1.17.0, which is where the L-07 fix was proven: after the fix the firmware emits
`[MAIN] Starting FreeRTOS scheduler` and then `[TASK] InputTask entered` through
`[TASK] DisplayTask entered`, with the sensor task repeating. Because Renode models neither
the SSD1306 nor the DHT22/LDR, the peripheral rows above are still recorded on the strength
of correct modelling and correct wiring, verified by inspection and by the diagram lint
(`tools/verify/run_all.sh` pass 6), rather than by an observed Wokwi run. The one row that is
a genuine finding rather than a status is the last: `BASEPRI` is not implemented by Wokwi at
all, which no amount of re-running will change.

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

> Tier 1 as listed describes what the simulator is *capable* of showing. The Wokwi CI token
> available at the time is configured but no longer authenticates, so no fixed-revision run
> could be taken; the `[TASK] <name> entered` banners have not been observed *under Wokwi*.
> They **have** been observed under Renode, which models SysTick, the NVIC and USART
> faithfully — see L-07 for the captured log. Wokwi remains the right place to demonstrate the peripherals it models
> and Renode does not (the SSD1306 and the DHT22), and vice versa.

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
| L-07 Gate never lifted / no scheduler | High | Pre-scheduler guard in both critical-section primitives | **Very Low — fix replayed under Renode, all five tasks observed running** |
| L-08 `BASEPRI` not implemented | Medium | None possible in simulation | Medium |
| L-09 Long-form labels dropped | Medium | Short header labels only; documented | Low |
| L-11 Mutex used before creation | Low | Create it before the first reporting path | Very Low |
| L-12 Unbounded I2C timeout | Medium | `OLED_I2C_TIMEOUT_MS` on every transaction | Very Low |
| L-13 `PendSV` yield from stock header | High | `vPortYieldFromISR()` via `extern`; asserted at build time | Very Low |

---

## Conclusion

All four primary project limitations (L-01 to L-04) are properly documented, technically justified, and accepted within the scope of Laboratory Activity 1. The simulation hurdles were diagnosed in sequence, and the sequence matters because each earlier fix masked the one behind it:

- **L-05 (UART)** — Wokwi silently discarded every wire written with a long-form pin label, so the serial monitor was never connected. Fixed by rewriting `diagram.json` with short header labels.
- **L-06 (OLED)** — the driver sent 1,024 individual I2C transactions per frame; refactored to a single bulk transfer. This is a genuine improvement, but it was **not** the fix that made the display work.
- **L-07 (the hang, and its real cause)** — the last and hardest of the sequence, and the only one that actually stopped the system. The port gates the kernel tick by clearing `SysTick->CTRL.TICKINT`, and `vPortExitCritical()` only lifts that gate when the nesting counter returns to exactly `0`. The counter starts at the sentinel `0xaaaaaaaa` ("kernel not started"), so a critical section taken before `vTaskStartScheduler()` walks it `0xAAAAAAAA → 0xAAAAAAAB → 0xAAAAAAAA` and never reaches `0`: the first critical section closes the gate for good. `main()` creates the UART mutex and five tasks before starting the scheduler, and those kernel calls take a critical section, so the gate closed at `UART_Mutex_Init()`; `OLED_Init()` then waited forever on the frozen `HAL_Delay()`. Both primitives now return early while `xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED`. **Replayed under Renode and confirmed fixed**: `uwTick` advances, SysTick is never left gated, and all five `[TASK] … entered` banners are emitted. An earlier revision of this document blamed Wokwi's `cpsie` instruction instead; that explanation was wrong and is withdrawn — and the reference implementation this project was compared against carries the *same* latent flaw, surviving only because it never calls `HAL_Delay` before its scheduler starts.
- **L-08 (`BASEPRI`)** — not implemented in Wokwi, so critical sections are not genuinely protected there. Accepted; `BASEPRI` works correctly on the target.
- **L-09 (silent wire discard)** — the defect class that made L-06's "resolved" claim false. Procedurally mitigated.
- **L-11 to L-13** — three further real defects surfaced by the same replay, none of which the Wokwi environment could have exposed: the UART mutex was reported through before it was created, the OLED transactions had no timeout, and an external-interrupt yield compiled against the stock header and would have vectored into `Default_Handler`. All three are fixed and the last two are now asserted on every build by `tools/verify/check_port_patch.py`.

**A note on one earlier claim.** Commit `647b343` is titled *"OLED now renders in Wokwi"*, and that claim was carried into this documentation for several revisions. It was never true: at that commit every pin label was long-form, so Wokwi had discarded the OLED wire and the display was not connected. The claim is corrected in L-06 rather than deleted, because a commit message that asserts an unobserved result is itself a defect — it converts an untested change into a believed one, and it makes any later regression impossible to diagnose.

For grading purposes, the functional requirements are verified through the combination of native unit tests (33 tests, all passing), static code analysis (0 functional defects; 21 clang-tidy advisories, 1 compiler warning, and 2 benign memory-mapped-register findings), the seven-pass verification harness in `tools/verify/run_all.sh` (all seven run on this machine and all pass, including the offline Wokwi diagram lint), and a **Renode 1.17.0 runtime replay in which the scheduler starts and all five tasks execute** (L-07). Wokwi remains the intended end-to-end simulator and the diagram lints cleanly offline, but the CI token available at the time no longer authenticates, so no Wokwi run could be taken against the fixed revision; Renode was used instead because it models the NVIC, SysTick and USART faithfully. The fault experiments (3 documented) record development-time observations and are not replayable.

**Evidence limits.** Three claims in this documentation could not be fully re-verified in the environment used to audit it. Each is flagged at its point of use. One of them has since been re-verified in full and one partially; the remainder is set out below.

1. **Firmware size (report §7.1).** *Re-verified in full.* An earlier audit pass could not repeat the PlatformIO build, because the vendor ARM toolchain and the STM32Cube/FreeRTOS sources were not present in that environment. The build has since been run against the current revision and the figures in §7.1 are the result: `pio run -e bluepill_f103c8` reports RAM 15,796 / 20,480 B (77.1%) and Flash 36,244 / 65,536 B (55.3%), matching `size -A` on the linked ELF (Flash = `.text` 33,024 + `.rodata` 2,732 + `.data` 488; RAM = `.data` 488 + `.bss` 15,308). This supersedes both the RAM 14,356 B / Flash 26,304 B originally published in §7.1 — measured before the queue fan-out fix (`9d0d056`) and the OLED bulk-transfer fix (`647b343`) and never re-measured, an understatement of 1,028 B of RAM and 4 B of Flash — and the intermediate pre-instrumentation baseline of RAM 15,384 B / Flash 26,308 B.

   The increase over that intermediate baseline is accounted for: the diagnostic instrumentation described in report §7.5 (`src/drivers/diag.c`: 1,216 B `.text`, 36 B `.bss`) and the `-Wl,-u,_printf_float` link flag, which pulls the newlib float formatter into the image at a cost of just under 3 KB. `tools/verify/run_size_analysis.sh` remains available as an application-only cross-check, but it uses clang's ARM target rather than `arm-none-eabi-gcc`, so its absolutes are indicative rather than authoritative.

2. **Simulator runs.** *Replayed under Renode; result positive.* An earlier audit could not reproduce the Wokwi observations in `docs/functional-verification.md` and recorded a negative result: the firmware stopped after its three boot lines. That symptom has since been root-caused and fixed (L-07), and the fix has been replayed under Renode 1.17.0 against the current revision, where the firmware reaches `[MAIN] Starting FreeRTOS scheduler` and enters all five tasks. Wokwi itself was not re-run against the fixed revision, because the CI token available at the time no longer authenticates (the CLI returns `API Error: Unauthorized`, and the same error is returned for a deliberately invalid token, so the failure is credential validity rather than the monthly run quota); the offline diagram lint does pass. Per-peripheral claims that Renode cannot exercise — the SSD1306 and the DHT22/LDR in particular, since Renode models neither — remain development-time observations and are marked as such in `docs/functional-verification.md`. The native unit tests and static analysis in `tools/verify/` reproduce exactly.

3. **Fault-experiment logs.** *Not replayable.* No logs were stored. Their *predicted* outcomes are derived deterministically from `src/FreeRTOSConfig.h` and the task priorities in `src/main.c`, so they hold regardless of any run; the *observed* sections record what was seen at development time. The two are labelled separately throughout `docs/fault-experiments.md`. Re-running them would require physical hardware, and a simulator cannot expose RTOS scheduling internals such as priority starvation in any case.

What **was** re-verified from scratch: all 33 unit tests were recompiled and re-run (33/33 pass, `tools/verify/run_tests.sh`); the static analysis was re-executed (0 functional defects, 21 clang-tidy advisories, 1 compiler warning, 2 benign MMIO findings — `tools/verify/run_static_analysis.sh`); the firmware was rebuilt with the vendor toolchain and its RAM and Flash footprint confirmed against both the PlatformIO summary and `size -A` on the linked ELF; and every file path, symbol name, constant, task priority, and queue name cited anywhere in this documentation was checked against `src/`.