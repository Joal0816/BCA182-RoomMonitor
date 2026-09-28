# Real-Time Multisensor Room Monitoring System with FreeRTOS

## Project Overview

A real-time room environment monitoring system built on the **STM32 Blue Pill** (STM32F103C8T6) using **FreeRTOS** and **PlatformIO**. The system monitors temperature, humidity, ambient light, and motion, displaying data on an SSD1306 OLED with rotary encoder navigation. Features an ACTIVE/INACTIVE state machine that suppresses the sensor display when the room is empty, and a temperature alarm with buzzer.

**GitHub Repository:** [github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)

---

## Motivation

Room monitoring systems are fundamental to IoT and smart building applications. This project demonstrates how to build a production-quality embedded system using a real-time operating system (RTOS), proper task scheduling, state machines, and modular software architecture. By documenting design decisions, testing strategies, and lessons learned, this project serves as a practical reference for learning embedded systems development with FreeRTOS on ARM Cortex-M microcontrollers.

---

## Features

- **Multi-sensor monitoring** — Temperature, humidity, light, and motion simultaneously
- **FreeRTOS architecture** — 5 concurrent tasks with defined priorities
- **State machine** — ACTIVE/INACTIVE modes with 15-second inactivity timeout
- **Rotary encoder navigation** — Intuitive page switching between sensor displays
- **SSD1306 OLED display** — Real-time sensor data visualization
- **Temperature alarm** — Configurable buzzer alerts for out-of-range temperatures
- **Modular design** — Clean separation of HAL drivers, logic, and tasks
- **Unit tested** — 33 automated tests with 100% pass rate
- **Static analysis verified** — Zero HIGH/MEDIUM findings

---

## Components Used

| Component | Purpose | Quantity |
|-----------|---------|----------|
| STM32 Blue Pill (STM32F103C8T6) | Main microcontroller | 1 |
| DHT22 | Temperature and humidity sensor | 1 |
| LDR (Photoresistor) | Ambient light detection | 1 |
| PIR Sensor (HC-SR501) | Motion detection | 1 |
| SSD1306 OLED (0.96", I2C) | Visual display | 1 |
| KY-040 Rotary Encoder | Menu navigation input | 1 |
| Buzzer | Audible alarm output | 1 |

---

## Circuit Design

### Pin Connections

| Component | STM32 Pin | Protocol |
|-----------|-----------|----------|
| DHT22 Data | PA1 | Digital (1-wire) |
| LDR Analog Out | PA0 | ADC1_CH0 |
| PIR Output | PB0 | Digital (EXTI0) |
| OLED SCL | PB6 | I2C1_SCL |
| OLED SDA | PB7 | I2C1_SDA |
| Encoder CLK | PA2 | Digital (EXTI2) |
| Encoder DT | PA3 | Digital Input |
| Encoder SW | PA4 | Digital (EXTI4) |
| Buzzer | PB8 | PWM (TIM4_CH3) |
| USART1 TX | PA9 | Serial Output |
| USART1 RX | PA10 | Serial Input |

---

## System Architecture

```
                    ┌─────────────────────────────┐
                    │      STM32 Blue Pill         │
                    │       (FreeRTOS)             │
                    └──────────┬──────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
  ┌──────────┐          ┌──────────┐          ┌──────────┐
  │SensorTask│          │InputTask │          │MotionTask│
  │  (Prio 2)│          │ (Prio 3) │          │ (Prio 3) │
  └────┬─────┘          └────┬─────┘          └────┬─────┘
       │                     │                     │
  ┌────▼─────┐          ┌────▼─────┐          ┌────▼─────┐
  │ DHT22    │          │ Encoder  │          │   PIR    │
  │ LDR      │          │          │          │          │
  └────┬─────┘          └────┬─────┘          └────┬─────┘
       │                     │                     │
       │              ┌──────▼──────┐              │
       │              │ Event Group │◄─────────────┘
       │              └──────┬──────┘
       │                     │
  ┌────▼─────────────────────▼────┐
  │         sensor_queue          │
  └────┬─────────────────────┬────┘
       │                     │
  ┌────▼─────┐          ┌────▼─────┐
  │AlarmTask │          │DisplayTask│
  │  (Prio 2)│          │  (Prio 1) │
  └────┬─────┘          └────┬─────┘
       │                     │
  ┌────▼─────┐          ┌────▼─────┐
  │  Buzzer  │          │   OLED   │
  └──────────┘          └──────────┘
```

---

## FreeRTOS Architecture

### Task Configuration

| Task | Responsibility | Trigger/Period | Priority | IPC | Blocked On |
|------|---------------|----------------|----------|-----|------------|
| SensorTask | Read DHT22 + LDR | 1s periodic | 2 | Queue | vTaskDelayUntil() |
| DisplayTask | Manage OLED | Event/update | 1 | Queue | Waiting for data |
| InputTask | Process encoder | Event-driven | 3 | Event Group | ulTaskNotifyTake() |
| MotionTask | Monitor PIR | Event-driven | 3 | Event Group | ulTaskNotifyTake() |
| AlarmTask | Alarm logic + buzzer | Sensor update | 2 | Queue | xQueueReceive() |

### Priority Justification

- **MotionTask & InputTask (Priority 3):** User-facing inputs require immediate response. Motion detection drives the state machine; encoder input drives display navigation.
- **SensorTask & AlarmTask (Priority 2):** Sensor readings are periodic and can tolerate slight jitter. Alarm evaluation depends on fresh sensor data.
- **DisplayTask (Priority 1):** Display updates are cosmetic. The OLED can lag behind sensor data without affecting system correctness.

### Synchronization Primitives

| Primitive | Type | Producer | Consumer | Purpose |
|-----------|------|----------|----------|---------|
| sensor_queue | Queue (depth 1) | SensorTask | AlarmTask | Latest SensorData_t |
| display_sensor_queue | Queue (depth 1) | SensorTask | DisplayTask | Latest SensorData_t |
| display_page_queue | Queue (depth 1) | InputTask | DisplayTask | Current display page |
| event_group | Event Group | MotionTask, InputTask | DisplayTask | `MOTION_DETECTED_BIT`, `ENCODER_CW_BIT`, `ENCODER_CCW_BIT`, `ENCODER_BTN_BIT` |
| uart_mutex | Mutex | Any task | UART1 | Protect serial output |

---

## How It Works

### Step 1: System Initialization
On power-up, HAL initializes clocks, GPIO, I2C, ADC, and UART peripherals. FreeRTOS creates all 5 tasks, queues, event groups, and mutexes, then starts the scheduler.

### Step 2: Sensor Data Acquisition
SensorTask reads DHT22 (temperature + humidity) via one-wire protocol, samples LDR through ADC (12-bit), and checks PIR digital output. It publishes the latest sample to independent alarm and display queues so both consumers receive every valid reading.

### Step 3: State Machine
StateMachine_Update() checks PIR sensor. If motion detected → ACTIVE mode. After 15 seconds without motion → INACTIVE mode (OLED shows `SYSTEM INACTIVE` instead of the sensor pages). Motion immediately restores ACTIVE.

### Step 4: Display Navigation
DisplayTask renders 4 pages based on encoder input:
- **Page 1:** Temperature with status (LOW/NORMAL/HIGH)
- **Page 2:** Humidity with progress bar
- **Page 3:** Light level with progress bar
- **Page 4:** Motion detection status

### Step 5: Temperature Alarm
AlarmTask evaluates temperature against thresholds:
- Temperature < 18°C → LOW alarm (buzzer at 1000Hz)
- Temperature > 30°C → HIGH alarm (buzzer at 2000Hz)
- 18°C ≤ Temperature ≤ 30°C → Normal (buzzer off)

---

## Software Design

### Modular Architecture

```
src/
├── main.c                 # Entry point, task creation
├── main.h                 # Common definitions, data structures
├── FreeRTOSConfig.h       # FreeRTOS configuration
├── stm32f1xx_hal_conf.h   # HAL configuration
├── app/
│   ├── hal/               # Hardware abstraction drivers
│   │   ├── dht22.c/.h     # DHT22 sensor driver
│   │   ├── ldr.c/.h       # LDR ADC driver
│   │   ├── pir.c/.h       # PIR motion driver
│   │   ├── oled.c/.h      # SSD1306 OLED driver
│   │   ├── encoder.c/.h   # Rotary encoder driver
│   │   └── buzzer.c/.h    # Buzzer PWM driver
│   ├── tasks/             # FreeRTOS task implementations
│   │   ├── sensor_task.c/.h
│   │   ├── display_task.c/.h
│   │   ├── input_task.c/.h
│   │   ├── motion_task.c/.h
│   │   └── alarm_task.c/.h
│   └── logic/             # Hardware-independent logic
│       ├── state_machine.c/.h
│       ├── temperature.c/.h
│       └── alarm.c/.h
└── drivers/
    └── uart_mutex.c/.h    # UART mutex wrapper
```

### Design Principles
- **Separation of concerns:** HAL drivers know nothing about application logic
- **Testability:** Pure logic functions can be unit tested without hardware
- **Modularity:** Each sensor, task, and logic module is self-contained

---

## Testing and Verification

### Unit Tests (33 total)

| Test Suite | Tests | What It Verifies |
|------------|-------|------------------|
| test_temperature | 15 | Boundary: <18°C, =18°C, normal, =30°C, >30°C |
| test_encoder | 10 | Navigation: increment, decrement, wrap-around |
| test_state_machine | 8 | Transitions: ACTIVE↔INACTIVE |

### Static Analysis
- **0 HIGH** severity findings
- **0 MEDIUM** severity findings
- **0 functional defects**, **21 LOW** severity clang-tidy advisories, plus **1** compiler sign-compare warning and **2** benign memory-mapped-register findings (all advisory — no corrective action required)

### Functional Verification (10 tests)
Ten functional checks cover temperature display, humidity display, light display, encoder navigation (CW/CCW), alarm activation/deactivation, and state machine transitions. Their expected outcomes were verified against the source; see the status note above regarding replay.

---

## Demonstration

The system is built for Wokwi with all 7 peripheral components wired:

> **Status note.** The circuit and firmware are complete, and the simulator deviations that
> were blocking the boot have been identified and worked around (see Challenge 4 below).
> The demonstration steps and serial transcript below record development-time observations.
> They have since been replayed against the current revision and were **not** reproduced:
> the simulator session stops after the three boot lines and no task was observed running,
> so the transcript below should be read as the intended behaviour rather than a current
> result. See `docs/limitations.md` L-07 for the full replay result. The 33 native unit
> tests and the static-analysis passes *do* reproduce, and all pass.

1. **Active Monitoring & Page Navigation:**
   - The SSD1306 OLED displays current environmental telemetry.
   - Turning the KY-040 rotary encoder cycles across the 4 display pages: **Page 0 (Temperature)**, **Page 1 (Humidity)**, **Page 2 (Ambient Light)**, and **Page 3 (Motion Detection)**.
   - Wraparound navigation works in both clockwise and counter-clockwise directions.

2. **Acoustic & Visual Alarm Activation:**
   - Setting the DHT22 temperature above 30.0°C triggers the buzzer PWM alert at PB8 (1000 Hz) within 2 seconds.
   - Returning the temperature to the 18–30°C band deactivates the alarm.

3. **Display-Suppression State Machine:**
   - Triggering the PIR motion sensor maintains the system in the **ACTIVE** state, and the OLED draws the currently selected sensor page.
   - After 15 seconds without detected motion, the system automatically transitions to **INACTIVE** state, and the OLED replaces the sensor page with a `SYSTEM INACTIVE` notice. No task is suspended and no clock is gated: the saving is in what is rendered and in the I²C traffic that rendering would otherwise generate, not in CPU time.
   - Any subsequent motion event immediately restores the system to **ACTIVE** mode within 100 ms.

4. **Telemetry Logging:**
   - Real-time diagnostic logs stream over USART1 (PA9, 115200 baud) without character corruption thanks to mutex synchronization.

---

## Challenges Encountered

### 1. Dual-Consumer Queue Contention
Initially, a single FreeRTOS queue was shared between `AlarmTask` and `DisplayTask`. Because FreeRTOS queues are destructive on read (`xQueueReceive()`), whichever task woke first consumed the sensor telemetry packet, causing the other task to starve until the next 1-second sampling interval.
- **Resolution:** Refactored into a dual-queue fan-out architecture (`sensor_queue` and `display_sensor_queue`), each of depth 1. `SensorTask` updates both using `xQueueOverwrite()`, guaranteeing that both consumers always have immediate access to the latest sample.

### 2. SSD1306 Virtual I2C Bus Bottleneck
The original OLED driver updated the screen by transmitting 1,024 individual I2C transactions (one for each pixel byte). This saturated Wokwi's virtual I2C engine and caused the display to freeze.
- **Resolution:** Rewrote `OLED_Update()` to transmit the full 1,025-byte frame buffer (`0x40` control byte + 1,024 data bytes) in a single bulk `HAL_I2C_Master_Transmit()` call, cutting I2C transaction overhead by 99.9%.

### 3. DHT22 Microsecond Timing in Critical Sections
The 1-wire protocol requires sub-millisecond bus timing. Calling `HAL_Delay()` inside a critical section locked the processor because the SysTick interrupt was masked.
- **Resolution:** Derived microsecond delays from the ARM Cortex-M3 Data Watchpoint and Trace cycle counter (`DWT->CYCCNT`), running at 72 MHz (13.88 ns per tick), so no interrupt is needed. The counter's enable bit lives in `DWT->CTRL`, which is a writable configuration register — it reads back set as soon as software sets it, whether or not `CYCCNT` actually advances. The delay routine now verifies that the counter is running before trusting it, and falls back to a calibrated `nop` loop otherwise.

### 3. Blocking Sensor Init Delayed the Whole Boot
`DHT22_Init()` waited 2 seconds for the sensor to stabilise before returning. Because `main()` calls it before `vTaskStartScheduler()`, that wait also postponed the first serial message and the start of `DisplayTask` — during which the board showed a blank OLED and an empty terminal.
- **Resolution:** Moved the settling wait out of the driver and into `SensorTask` as a `vTaskDelay(SENSOR_SETTLE_MS)`, so it costs nothing on the boot path. Initialisation of an individual sensor must never gate the rest of the firmware.

### 4. Wokwi's `cpsie` Masked the Scheduler's Own Supervisor Call
After the boot-blocking defect above was fixed, the terminal still showed only two lines — `[MAIN] System initialized` and `[MAIN] Starting FreeRTOS scheduler` — followed by nothing at all. Both are printed from `main()` *before* `vTaskStartScheduler()`, so the silence began exactly at the scheduler start. The OLED stayed blank and no task ever ran.

> **Confidence note.** This section describes a mechanism that is **consistent with** the
> observed symptom, not one that was observed directly. The comparison below is stated as
> what it was: a leading hypothesis reached by reading the port's source and disassembly,
> after every other candidate had been excluded. A replay against the current revision still
> does not produce a running system even with the workaround applied, so the workaround is
> unproven. See Challenge 4's evidence status in the full report and `docs/limitations.md`
> L-07.

The FreeRTOS Cortex-M3 port's `prvPortStartFirstTask()` clears the interrupt masks and then issues the supervisor call that starts the first task:

```asm
cpsie i          ; clear PRIMASK  -- enable interrupts
cpsie f          ; clear FAULTMASK
dsb
isb
svc 0            ; start the first task
```

On real ARMv7-M hardware `cpsie` **clears** those masks. The hypothesis is that Wokwi's Cortex-M3 model implements it with the opposite effect, **setting** them. `PRIMASK` would then be left at 1 when the `svc 0` executed, and because `SVCall` is a configurable-priority exception it is masked by `PRIMASK` — the supervisor call never fires, the first task is never entered, and the system sits in the idle loop of `xPortStartScheduler()` forever with interrupts masked: no tick, no context switch, no output.

Everything above this layer had already been eliminated: the wiring resolved to real pins, the pre-scheduler UART output proved the HSE oscillator and 72 MHz PLL were locked, the heap had ~2 KB of headroom against a 12,288 B pool, and the ISR priorities were all legal. The fault was in the simulator's instruction semantics, so no application-level change could have fixed it.

- **Resolution:** A project-local copy of the V11.3.1 ARM_CM3 port at `lib/freertos_port_patch/src/port.c` adds three instructions before the `svc`:

```asm
movs r0, #0
msr primask, r0      ; clear PRIMASK explicitly
msr faultmask, r0    ; clear FAULTMASK explicitly
svc 0
```

`msr` is the architecturally correct way to clear these masks, and the added instructions are redundant no-ops on real hardware. The `cpsie` instructions are deliberately **kept** rather than removed, so the port stays correct on genuine silicon and the deviation is confined to three instructions that are harmless there. `library.json` sets `"libArchive": false` so the patched object links as a plain object and a duplicate symbol fails the build loudly rather than silently falling back to the unpatched archive member, and `tools/verify/check_port_patch.py` warns post-build if the patch is missing from the image.

### 5. Silent Faults Were Indistinguishable From Hangs
The default Cortex-M startup aliases every fault handler to a bare infinite loop, so a HardFault looked exactly like a hang: no output, no error, no indication of cause. This is what made the defect above expensive to find, and it was addressed independently of it.
- **Resolution:** Added `src/drivers/diag.c`, which installs a `HardFault_Handler` that decodes the stacked exception frame and prints the fault status registers (`CFSR`, `HFSR`, `MMFAR`, `BFAR`) over USART1 by writing `USART1->DR` directly — it cannot use the normal logging path, which takes a mutex and would deadlock in a fault context. `main()` now also captures every `xTaskCreate()` return code, each task prints a banner on entry, and a post-scheduler guard reports if `vTaskStartScheduler()` ever returns. An LED on PC13 makes the existing fault hooks visible on the simulated board.

### 6. Sensor Failure False Alarms
If a sensor read timed out or suffered parity error, default zero values triggered an erroneous LOW temperature alarm (<18°C).
- **Resolution:** Flagged failed reads with `NAN` and verified validity via `isnan()` before evaluating alarm thresholds.

---

## Lessons Learned

1. **`vTaskDelayUntil()` vs `vTaskDelay()`:** `vTaskDelay()` introduces accumulated drift over time equal to task execution duration. `vTaskDelayUntil()` calculates delays relative to the scheduled start time, ensuring zero cumulative drift for periodic sensing.
2. **IPC Mechanism Selection:** Using a mutex for UART output prevents race conditions and interleaved text. Using an event group allows atomic multi-event signaling (motion, rotation, button press) without polling.
3. **Queue Ownership:** Multi-consumer architectures require dedicated queues per consumer or a broadcast/overwrite design rather than a single shared FIFO queue.
4. **Hardware Abstraction Layer (HAL) Isolation:** Decoupling decision logic (`src/app/logic/`) from peripheral drivers (`src/app/hal/`) enables comprehensive native unit testing on host PCs (33/33 tests pass without target hardware).
5. **A silent failure is worse than a loud one.** On Cortex-M the default fault handlers are infinite loops, so a faulting board and a hung board are indistinguishable. Installing a handler that reports `CFSR`/`HFSR` converts an unobservable failure into a diagnosable one.
6. **Where output stops is the most valuable clue you have.** The symptom — two lines printed before `vTaskStartScheduler()` and nothing after — located the fault at the scheduler boundary and ruled out every task, driver, and peripheral above it. Finding that boundary first would have saved most of the debugging time.
7. **Do not trust a commit message as evidence.** An earlier commit claimed the OLED rendered in Wokwi; it never had, because the wires had been silently discarded. A claim of a verified result that was never observed turns an untested change into a believed one and makes later regressions undiagnosable.

---

## Limitations

1. **DHT22 Blocking Read (~20 ms):** The single-wire read sequence runs inside `taskENTER_CRITICAL()`, holding the scheduler off for roughly 20 ms. That is 2% of the 1-second sampling period and is deemed acceptable; the critical section raises BASEPRI rather than masking all interrupts, so SysTick still ticks.
2. **Single Buzzer Alarm:** Currently only temperature out-of-range conditions sound the buzzer; humidity and motion alarms are visual-only.
3. **No Persistent Storage:** Telemetry is stored purely in volatile RAM; power cycling clears historical data.
4. **Fixed Task Priorities:** Task priorities are configured statically at compile time rather than dynamically adapted.
5. **Simulator Fidelity:** Wokwi's Cortex-M3 model is believed to deviate from ARMv7-M in three ways that affect this project — `cpsie` sets the interrupt masks instead of clearing them, `BASEPRI` is not implemented, and a pin label it does not recognise is discarded without warning. The first two mean that interrupt handling and critical sections cannot be genuinely verified in simulation, and the third cost real debugging time. The firmware is written for correct hardware behaviour; the simulator deviations are worked around, not accommodated in the design. A replay against the current revision still does not reach a running task even with the workaround in place, so the on-target behaviour remains to be confirmed on real hardware.
6. **Float Formatting:** The `newlib-nano` default links an integer-only `printf`, so `%.1f` silently emitted literal conversion text rather than a number. Fixed with `-Wl,-u,_printf_float`; the cost is a little under 3 KB of flash.
7. **I2C Error Reporting (addressed):** The OLED driver originally discarded the status returned by every `HAL_I2C_Master_Transmit()` call, so the firmware could not distinguish "display acknowledged" from "display absent" — a missing or mis-addressed display presented as a blank screen rather than an error. `OLED_Init()` and `OLED_Update()` now return `HAL_StatusTypeDef`, latch the first failure, and the callers in `main.c` and `DisplayTask()` print `[OLED] init failed` / `[OLED] frame transfer failed` over UART. The remaining gap is that a *transient* failure is reported but not retried.

---

## Future Improvements

- **Wi-Fi / MQTT Connectivity:** Integrate an ESP8266 or ESP32 to publish sensor telemetry to an MQTT broker or cloud dashboard.
- **MicroSD Data Logging:** Implement an SPI SD card interface with a FAT32 filesystem for non-volatile sensor logging.
- **Low-Power Sleep Modes:** Enter STM32 STOP or STANDBY mode during INACTIVE state with PIR EXTI wakeup to minimize battery draw.
- **Multi-Tone Acoustic Alerts:** Differentiate alarm conditions by driving the buzzer at distinct PWM frequencies (e.g., 1000 Hz for temperature, 2000 Hz for humidity).

---

## GitHub Repository

Complete source code, PlatformIO configuration, Wokwi diagram, unit tests, and documentation:
- **Repository URL:** [https://github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)

---

## References

1. FreeRTOS Kernel Developer Guide: [https://www.freertos.org/](https://www.freertos.org/)
2. STMicroelectronics STM32F103x8 Reference Manual (RM0008)
3. SSD1306 128x64 Dot Matrix OLED Controller Datasheet
4. Wokwi Simulator Documentation: [https://docs.wokwi.com/](https://docs.wokwi.com/)

---

*Built as part of BCA182 Embedded Systems Programming — Mindanao State University - Iligan Institute of Technology*
