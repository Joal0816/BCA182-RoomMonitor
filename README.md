# BCA182 Real-Time Multisensor Room Monitoring System

## Project Overview

A real-time room-environment monitoring system built on the **STM32 Blue Pill**
(STM32F103C8T6) using **native FreeRTOS**, the **STM32Cube HAL** and **PlatformIO**.
The system samples temperature, humidity, ambient light and motion, presents one
measurement at a time on an SSD1306 OLED selected with a rotary encoder, and
sounds a buzzer when the temperature leaves the configured safe range. A
power-saving state machine blanks the display after 15 s without motion and
restores it as soon as motion is detected.

The firmware is simulated in **Wokwi** and is organized as a concurrent,
modular application: driver code, hardware-independent decision logic and
FreeRTOS tasks are separated, and the decision logic is covered by host unit
tests.

**Repository:** [github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)

---

## Features

- **Multi-sensor monitoring** — DHT22 (temperature/humidity), LDR (light), PIR (motion)
- **Six concurrent FreeRTOS tasks** with explicit, justified priorities
- **Rotary-encoder navigation** across four OLED pages (Temperature, Humidity, Light, Motion)
- **Temperature alarm** — buzzer pattern for below 18 °C and above 30 °C
- **ACTIVE / INACTIVE state machine** — 15 s inactivity timeout blanks the OLED; motion restores it
- **Inter-task communication** — queues, an event group, task notifications and a mutex
- **Modular design** — HAL drivers, pure logic and tasks are separate translation units
- **Host unit tests** — 33 tests that link the real decision logic
- **Static analysis** — `pio check`, 0 HIGH / 0 MEDIUM findings

---

## Learning Objectives

This project demonstrates the learning outcomes of BCA182 Laboratory Activity 1:

1. Creating an STM32 project with PlatformIO and the STM32Cube framework.
2. Building and simulating a microcontroller circuit in Wokwi.
3. Interfacing sensors and actuators with an STM32.
4. Organizing firmware into multiple source modules.
5. Creating and managing FreeRTOS tasks, priorities and task states.
6. Using queues for inter-task communication.
7. Using a mutex to protect a shared resource.
8. Using an event group and task notifications for event signaling.
9. Implementing periodic execution with `vTaskDelayUntil()`.
10. Implementing a simple embedded-system state machine.
11. Separating hardware-independent decision logic from hardware drivers.
12. Writing and running automated unit tests.
13. Performing static code analysis.
14. Using Git incrementally and maintaining a professional repository.
15. Documenting the project for academic assessment and a public portfolio.

---

## System Architecture

```
                              ┌─────────────────────────────┐
                              │        STM32 Blue Pill       │
                              │     STM32F103C8T6 + FreeRTOS │
                              └──────────────┬──────────────┘
                                             │
        ┌──────────────┬──────────────┬──────┴───────┬──────────────┐
        ▼              ▼              ▼              ▼              ▼
 ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐
 │ SensorTask │ │ MotionTask │ │ InputTask  │ │  StateTask │ │ AlarmTask  │
 │   Prio 2   │ │   Prio 3   │ │   Prio 3   │ │   Prio 2   │ │   Prio 2   │
 └─────┬──────┘ └─────┬──────┘ └─────┬──────┘ └─────┬──────┘ └─────┬──────┘
       │              │              │              │              │
   DHT22/LDR     PIR (EXTI0)   Encoder(EXTI2/4)  state machine    Buzzer
       │              │              │              │              │
       │         event_group    display_page_q   event_group        │
       │              │              │              │              │
       ├──────────────┴──────────────┴──────────────┴──────────────┤
       │                     alarm_queue / display_queue            │
       ▼                                                           ▼
 ┌────────────┐                                             ┌──────────┐
 │DisplayTask │◄────────────── event_group ─────────────────│  OLED    │
 │   Prio 1   │               (ACTIVE bit)                  └──────────┘
 └────────────┘
```

The diagram above is the task/deployment view. A rendered project cover image is
available at `docs/cover-image.png`; a rendered state-machine figure and a
task-communication figure are part of the laboratory report and this README.

---

## FreeRTOS Architecture

### Task Table

| Task | Responsibility | Trigger / Period | Priority | IPC | Typical Blocked Condition |
|------|----------------|------------------|----------|-----|---------------------------|
| `SensorTask` | Read DHT22 + LDR, publish sample | 1 s periodic | 2 | Two queues (`alarm_queue`, `display_queue`) | `vTaskDelayUntil()` |
| `AlarmTask` | Evaluate temperature, drive buzzer | Sensor update | 2 | Queue (`alarm_queue`) | `xQueueReceive()` |
| `StateTask` | Own ACTIVE/INACTIVE state machine | 250 ms poll / motion event | 2 | Event group (`EVENT_MOTION_BIT` in, `EVENT_STATE_ACTIVE_BIT` out) | `xEventGroupWaitBits()` |
| `InputTask` | Rotary-encoder navigation | Encoder EXTI notification | 3 | Task notification + queue (`display_page_queue`) | `ulTaskNotifyTake()` |
| `MotionTask` | Monitor PIR, publish motion event | PIR EXTI / 500 ms refresh | 3 | Event group (`EVENT_MOTION_BIT`) | `ulTaskNotifyTake()` |
| `DisplayTask` | Sole owner of the OLED | 100 ms refresh | 1 | Queues (`display_queue`, `display_page_queue`) + event group read | `xQueueReceive()` |

### Priority Justification

Priorities express **scheduling urgency and acceptable latency**, not subjective
importance:

- **InputTask & MotionTask (3)** — user-facing, event-driven inputs. Motion drives
  the state machine and the encoder drives navigation. A delay here is perceived
  as lag, so they run above the periodic workers. Both block on notifications,
  so they consume no CPU while idle.
- **SensorTask, AlarmTask & StateTask (2)** — periodic/derived work. Sampling and
  alarm evaluation tolerate milliseconds of jitter; the state machine is polled.
  They must outrank the display so that fresh data and safety decisions are ready
  before rendering.
- **DisplayTask (1)** — cosmetic output. It may lag behind sensor data without
  affecting correctness, so it runs lowest and never starves the workers.

If a periodic worker were given a higher priority than the event tasks, encoder
and motion responses would become sluggish; if display ran highest, it would add
latency to sampling and alarm evaluation (demonstrated in Fault Experiment 2).

### Task States

While blocked (`vTaskDelayUntil`, `xQueueReceive`, `xEventGroupWaitBits`,
`ulTaskNotifyTake`) a task is in the **Blocked** state and yields the CPU. When
an event unblocks it, it becomes **Ready**; the scheduler puts it in the
**Running** state when it is the highest-priority ready task. No task contains an
uncontrolled busy loop.

---

## Hardware / Simulated Components

| Component | Purpose | Interface |
|-----------|---------|-----------|
| STM32 Blue Pill (STM32F103C8T6) | Main microcontroller | — |
| DHT22 | Temperature and humidity | 1-wire digital |
| Photoresistor (LDR) | Ambient light | ADC |
| PIR (HC-SR501) | Motion detection | Digital / EXTI |
| SSD1306 OLED 128×64 | Information display | I2C |
| KY-040 rotary encoder | Page navigation | Digital / EXTI |
| Buzzer | Alarm output | PWM |

---

## Pin Configuration

| Component | STM32 Pin | Peripheral |
|-----------|-----------|------------|
| DHT22 data | PA1 | GPIO |
| LDR analog | PA0 | ADC1 channel 0 |
| PIR output | PB0 | EXTI0 |
| OLED SCL | PB6 | I2C1_SCL |
| OLED SDA | PB7 | I2C1_SDA |
| Encoder CLK | PA2 | EXTI2 |
| Encoder DT | PA3 | GPIO input |
| Encoder SW | PA4 | EXTI4 |
| Buzzer | PB8 | TIM4_CH3 (PWM) |
| USART1 TX/RX | PA9 / PA10 | USART1 (115200 8N1) |
| Status LED | PC13 | GPIO |

> Peripheral clocks and GPIO alternate functions are set up by the STM32Cube MSP
> callbacks in `src/stm32f1xx_hal_msp.c`. In `diagram.json`, Wokwi refers to the
> same pins with short labels (`A1`, `B6`, `B7`, `B8`, `A9`, `A10`) and the power
> rails `3V3.1`, `5V.1` and `GND.1`.

---

## Task Design

- **SensorTask** performs finite work (read sensors, publish samples) and then
  blocks in `vTaskDelayUntil()`. It is the task that satisfies the periodic-timing
  requirement.
- **MotionTask** and **InputTask** are event-driven: the PIR and encoder EXTI
  handlers call `vTaskNotifyGiveFromISR()`, and the tasks block in
  `ulTaskNotifyTake()`. MotionTask additionally refreshes every 500 ms so that
  sustained motion keeps the event bit asserted.
- **AlarmTask** blocks on `alarm_queue` and hands the temperature status to the
  pure alarm logic, which toggles the buzzer.
- **StateTask** centralizes the ACTIVE/INACTIVE machine and publishes the state
  through the event group so that the `StateMachine_t` is only ever touched by a
  single task.
- **DisplayTask** owns the OLED. It drains the sensor and page queues and reads
  the ACTIVE bit; it is the only task that calls the OLED driver.

---

## Inter-Task Communication

| Primitive | Type | Producer | Consumer | Purpose |
|-----------|------|----------|----------|---------|
| `alarm_queue` | Queue (depth 5) | SensorTask | AlarmTask | Every sensor sample for alarm evaluation |
| `display_queue` | Queue (depth 5) | SensorTask | DisplayTask | Every sensor sample for rendering |
| `display_page_queue` | Queue overwrite (depth 1) | InputTask | DisplayTask | Latest selected page |
| `event_group` | Event group | MotionTask, StateTask | StateTask, DisplayTask | Motion and ACTIVE state signaling |
| `uart_mutex` | Mutex | All tasks | USART1 | Serialize multi-character UART writes |
| Task notification | Direct-to-task | PIR / encoder EXTI (ISR) | MotionTask / InputTask | Fast wake-up on hardware events |

### Event Group Bits

| Bit | Producer | Consumer | Meaning |
|-----|----------|----------|---------|
| `EVENT_MOTION_BIT` | MotionTask | StateTask | PIR motion currently present |
| `EVENT_STATE_ACTIVE_BIT` | StateTask | DisplayTask | System is ACTIVE (OLED on) |
| `EVENT_ALARM_BIT` | AlarmTask | Diagnostics | Reserved for alarm-state signaling |

The two sensor consumers receive **independent queues** so that neither can
consume a sample intended for the other.

---

## State Machine

```
                    ┌───────────────────┐
      motion        │                   │
   ┌───────────────►│      ACTIVE       │
   │                │  OLED on          │
   │                │  Alarm active     │
   │                │  Navigation active│
   │                └─────────┬─────────┘
   │                          │ 15 s without motion
   │                          ▼
   │                ┌───────────────────┐
   └────────────────│     INACTIVE      │
      motion        │  OLED off/blank   │
                    │  Display ops ↓    │
                    │  Motion stays on  │
                    └───────────────────┘
```

The transition logic lives in `src/app/logic/state_machine.c` and is unit
tested independently of hardware.

---

## Repository Structure

```
BCA182-RoomMonitor/
├── src/
│   ├── main.c / main.h              # app_main(), init, RTOS object + task creation
│   ├── FreeRTOSConfig.h
│   ├── stm32f1xx_hal_conf.h
│   ├── app/
│   │   ├── hal/                     # Hardware drivers (DHT22, LDR, PIR, OLED, encoder, buzzer)
│   │   ├── logic/                   # Hardware-independent logic
│   │   │   ├── temperature.c/.h     # EvaluateTemperature()
│   │   │   ├── state_machine.c/.h   # StateMachine_Update()
│   │   │   ├── display_page.c/.h    # DisplayPage_Next()/Previous()
│   │   │   └── alarm.c/.h           # Buzzer alarm state
│   │   └── tasks/                   # FreeRTOS tasks
│   │       ├── sensor_task.c/.h
│   │       ├── display_task.c/.h
│   │       ├── input_task.c/.h
│   │       ├── motion_task.c/.h
│   │       ├── alarm_task.c/.h
│   │       └── state_task.c/.h
│   └── drivers/uart_mutex.c/.h      # Mutex-protected UART wrapper
├── include/FreeRTOSConfig.h
├── test/
│   ├── test_temperature/            # 15 temperature/alarm logic tests
│   ├── test_navigation/             # 10 encoder-navigation tests
│   └── test_state_machine/          # 8 ACTIVE/INACTIVE tests
├── docs/
│   ├── laboratory-report.pdf        # Academic report
│   ├── functional-verification.md   # Wokwi functional test record
│   ├── fault-experiments.md         # Deliberate FreeRTOS fault experiments
│   ├── static-analysis.md           # pio check findings and interpretation
│   ├── requirements-traceability.md # FR → implementation → verification
│   └── hackster-article.md          # Public portfolio write-up
├── diagram.json                     # Wokwi circuit
├── wokwi.toml                       # Wokwi firmware configuration
├── platformio.ini
└── README.md
```

---

## Getting Started

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or PlatformIO IDE for VS Code
- Git
- The Wokwi for VS Code extension (for simulation)
- (Optional) ST-Link V2 for physical hardware

### Building the Project

```bash
git clone https://github.com/Joal0816/BCA182-RoomMonitor.git
cd BCA182-RoomMonitor
pio run -e bluepill_f103c8
```

### Running the Wokwi Simulation

1. Install the **Wokwi Simulator** extension in VS Code.
2. Build the firmware with `pio run -e bluepill_f103c8`.
3. Open the project folder in VS Code (the root `diagram.json` and `wokwi.toml`
   are used automatically), then enable Wokwi and start the simulation.
4. The runtime logs appear in the Wokwi terminal. Interact with the DHT22,
   photoresistor, PIR and rotary encoder to exercise the system.

> The Wokwi firmware is the real STM32Cube build; `wokwi.toml` points at
> `.pio/build/bluepill_f103c8/firmware.elf`.

### Unit Testing

```bash
pio test -e native
```

Tests run on the host PC; they compile and link the real hardware-independent
logic from `src/app/logic/`.

### Static Code Analysis

```bash
pio check -e bluepill_f103c8
```

---

## Unit Testing

| Test Suite | Tests | What It Verifies |
|------------|-------|------------------|
| `test_temperature` | 15 | Below/at/above both thresholds, status strings, alarm predicate |
| `test_navigation` | 10 | Forward/reverse transitions and both wraparounds |
| `test_state_machine` | 8 | ACTIVE↔INACTIVE, timeout boundary, motion reset |
| **Total** | **33** | 100% pass |

The tests link `temperature.c`, `state_machine.c` and `display_page.c` directly,
so they fail if the production logic changes behaviour.

---

## Static Code Analysis

```bash
pio check -e bluepill_f103c8
```

**Results:** 0 HIGH, 0 MEDIUM, LOW-severity style findings only (see
`docs/static-analysis.md` for the full findings table and interpretation).

---

## Functional Verification

The functional tests follow the Wokwi procedures in
`docs/functional-verification.md`:

| ID | Stimulus | Expected Result |
|----|----------|-----------------|
| FT-01 | Change temperature | Displayed temperature updates |
| FT-02 | Change humidity | Displayed humidity updates |
| FT-03 | Change light input | Light value changes |
| FT-04 | Rotate encoder clockwise | Next page selected |
| FT-05 | Rotate encoder counterclockwise | Previous page selected |
| FT-06 | Set temperature above 30 °C | Alarm activates |
| FT-07 | Return temperature to normal | Alarm stops |
| FT-08 | Trigger PIR | System is ACTIVE |
| FT-09 | Allow inactivity timeout | System becomes INACTIVE |
| FT-10 | Trigger PIR while INACTIVE | System returns to ACTIVE |

---

## Engineering Decisions

### Why `vTaskDelayUntil()` instead of `vTaskDelay()`

`vTaskDelay()` delays relative to the moment it is called, so any execution time
before the call is added to every period and the sample time drifts. `vTaskDelayUntil()`
takes an absolute wake time that is advanced by exactly one period each cycle, so
the sampling instant stays fixed even if reading the sensors takes a variable
amount of time. For periodic sensor sampling this prevents accumulated drift.

### Why separate HAL drivers from logic

Decision logic (temperature evaluation, state machine, navigation) has no HAL or
FreeRTOS dependency, so it can be compiled and tested on the host. Drivers are
kept thin so the same logic can run on hardware or in simulation.

### Why an event group

Motion is produced by MotionTask and consumed by StateTask, and the resulting
ACTIVE state is consumed by DisplayTask. An event group lets one producer set
bits and multiple consumers wait on them without polling, and bits can be set
from ISR-safe contexts.

### Why a mutex for UART

`HAL_UART_Transmit()` writes a multi-byte string. If two tasks transmit
concurrently, their bytes interleave and the log becomes unreadable. The mutex
serializes whole messages.

### Why a dedicated StateTask

The activity recommends centralizing state management. A single task owns the
`StateMachine_t`, which removes the shared-mutable-state race that would exist if
several tasks read and wrote the state directly.

---

## Limitations

- **LDR is uncalibrated** — readings are raw 12-bit ADC values (0–4095), not lux.
  The display shows the raw value and a proportional bar.
- **RAM headroom** — the firmware uses most of the STM32F103C8's 20 KB SRAM, so
  the FreeRTOS heap is sized deliberately and task stacks are kept small.
- **DHT22 timing** — the one-wire read masks interrupts for a short window,
  which can add minor jitter to other tasks.
- **Simulation fidelity** — sensor behaviour in Wokwi approximates real hardware.

---

## Future Improvements

- Add connectivity (Wi-Fi/BLE) for remote monitoring.
- Persist timestamped sensor data to flash or an SD card.
- Implement adaptive alarm thresholds and a snooze control.
- Add more environmental sensors (pressure, air quality).
- Migrate shared logs to a queue-backed logger task.

---

## References and Acknowledgments

- [FreeRTOS Documentation](https://www.freertos.org/Documentation.html)
- [STM32 HAL / STM32CubeF1](https://www.st.com/en/embedded-software/stm32cubef1.html)
- [PlatformIO STM32Cube framework](https://docs.platformio.org/en/latest/frameworks/stm32cube.html)
- [Wokwi Documentation](https://docs.wokwi.com/)
- **Asst. Prof. Paul Rodolf P. Castor, M.Sc.** — course instructor, BCA182
- **Mindanao State University – Iligan Institute of Technology**, College of Computer Studies, Department of Computer Applications
