# Real-Time Multisensor Room Monitoring System with FreeRTOS

## Project Overview

A real-time room environment monitoring system built on the **STM32 Blue Pill** (STM32F103C8T6) using **FreeRTOS** and **PlatformIO**. The system monitors temperature, humidity, ambient light, and motion, displaying data on an SSD1306 OLED with rotary encoder navigation. Features a power-saving ACTIVE/INACTIVE state machine and temperature alarm with buzzer.

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
| alarm_sensor_queue | Queue (depth 1) | SensorTask | AlarmTask | Latest SensorData_t |
| display_sensor_queue | Queue (depth 1) | SensorTask | DisplayTask | Latest SensorData_t |
| display_page_queue | Queue (depth 1) | InputTask | DisplayTask | Current display page |
| event_group | Event Group | MotionTask, InputTask | DisplayTask | Motion/encoder events |
| uart_mutex | Mutex | Any task | UART1 | Protect serial output |

---

## How It Works

### Step 1: System Initialization
On power-up, HAL initializes clocks, GPIO, I2C, ADC, and UART peripherals. FreeRTOS creates all 5 tasks, queues, event groups, and mutexes, then starts the scheduler.

### Step 2: Sensor Data Acquisition
SensorTask reads DHT22 (temperature + humidity) via one-wire protocol, samples LDR through ADC (12-bit), and checks PIR digital output. It publishes the latest sample to independent alarm and display queues so both consumers receive every valid reading.

### Step 3: State Machine
StateMachine_Update() checks PIR sensor. If motion detected → ACTIVE mode. After 15 seconds without motion → INACTIVE mode (OLED blank, reduced operations). Motion immediately restores ACTIVE.

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
- **111 LOW** severity (style warnings only)

### Functional Verification (10 tests)
All 10 functional tests pass, covering temperature display, humidity display, light display, encoder navigation (CW/CCW), alarm activation/deactivation, and state machine transitions.

---

## Demonstration

The system was simulated and fully verified in Wokwi with all 7 peripheral components active:

1. **Active Monitoring & Page Navigation:**
   - The SSD1306 OLED displays current environmental telemetry.
   - Turning the KY-040 rotary encoder cycles smoothly across the 4 display pages: **Page 0 (Temperature)**, **Page 1 (Humidity)**, **Page 2 (Ambient Light)**, and **Page 3 (Motion Detection)**.
   - Wraparound navigation works seamlessly in both clockwise and counter-clockwise directions.

2. **Acoustic & Visual Alarm Activation:**
   - Setting the DHT22 temperature above 30.0°C triggers the buzzer PWM alert at PB8 (1000 Hz) within 2 seconds.
   - Lowering the temperature below 28.0°C deactivates the alarm.

3. **Power-Saving State Machine:**
   - Triggering the PIR motion sensor maintains the system in the **ACTIVE** state (OLED on, full sensor polling).
   - After 15 seconds without detected motion, the system automatically transitions to **INACTIVE** state, turning off the OLED display to conserve energy while leaving PIR interrupt monitoring active.
   - Any subsequent motion event immediately restores the system to **ACTIVE** mode within 100 ms.

4. **Telemetry Logging:**
   - Real-time diagnostic logs stream over USART1 (PA9, 115200 baud) without character corruption thanks to mutex synchronization.

---

## Challenges Encountered

### 1. Dual-Consumer Queue Contention
Initially, a single FreeRTOS queue was shared between `AlarmTask` and `DisplayTask`. Because FreeRTOS queues are destructive on read (`xQueueReceive()`), whichever task woke first consumed the sensor telemetry packet, causing the other task to starve until the next 2-second sampling interval.
- **Resolution:** Refactored into a dual-queue fan-out architecture (`alarm_sensor_queue` and `display_sensor_queue`), each of depth 1. `SensorTask` updates both using `xQueueOverwrite()`, guaranteeing that both consumers always have immediate access to the latest sample.

### 2. SSD1306 Virtual I2C Bus Bottleneck
The original OLED driver updated the screen by transmitting 1,024 individual I2C transactions (one for each pixel byte). This saturated Wokwi's virtual I2C engine and caused the display to freeze.
- **Resolution:** Rewrote `OLED_Update()` to transmit the full 1,025-byte frame buffer (`0x40` control byte + 1,024 data bytes) in a single bulk `HAL_I2C_Master_Transmit()` call, cutting I2C transaction overhead by 99.9%.

### 3. DHT22 Microsecond Timing in Critical Sections
The 1-wire protocol requires sub-millisecond bus timing. Calling `HAL_Delay()` inside a critical section locked the processor because the SysTick interrupt was masked.
- **Resolution:** Used the ARM Cortex-M3 Data Watchpoint and Trace cycle counter (`DWT->CYCCNT`) running at 72 MHz (13.88 ns per tick) for precise microsecond delays without relying on interrupts.

### 4. Sensor Failure False Alarms
If a sensor read timed out or suffered parity error, default zero values triggered an erroneous LOW temperature alarm (<18°C).
- **Resolution:** Flagged failed reads with `NAN` and verified validity via `isnan()` before evaluating alarm thresholds.

---

## Lessons Learned

1. **`vTaskDelayUntil()` vs `vTaskDelay()`:** `vTaskDelay()` introduces accumulated drift over time equal to task execution duration. `vTaskDelayUntil()` calculates delays relative to the scheduled start time, ensuring zero cumulative drift for periodic sensing.
2. **IPC Mechanism Selection:** Using a mutex for UART output prevents race conditions and interleaved text. Using an event group allows atomic multi-event signaling (motion, rotation, button press) without polling.
3. **Queue Ownership:** Multi-consumer architectures require dedicated queues per consumer or a broadcast/overwrite design rather than a single shared FIFO queue.
4. **Hardware Abstraction Layer (HAL) Isolation:** Decoupling decision logic (`src/app/logic/`) from peripheral drivers (`src/app/hal/`) enables comprehensive native unit testing on host PCs (33/33 tests passed in CI without target hardware).

---

## Limitations

1. **DHT22 Blocking Read (~5 ms):** The single-wire read sequence holds the CPU for ~5 ms with interrupts masked. This represents only 0.25% of the 2-second sampling period and is deemed acceptable.
2. **Single Buzzer Alarm:** Currently only temperature out-of-range conditions sound the buzzer; humidity and motion alarms are visual-only.
3. **No Persistent Storage:** Telemetry is stored purely in volatile RAM; power cycling clears historical data.
4. **Fixed Task Priorities:** Task priorities are configured statically at compile time rather than dynamically adapted.

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
