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

## Challenges and Lessons Learned

### 1. DHT22 Timing in Critical Section
The DHT22 one-wire protocol requires precise microsecond timing. Using `taskENTER_CRITICAL()` with `HAL_Delay()` caused system hang because SysTick was blocked. **Solution:** Replaced `HAL_Delay()` with DWT cycle-counting (`DWT->CYCCNT`) for microsecond delays inside critical sections.

### 2. DHT22 Failure Triggers False Alarm
When DHT22 read fails, temperature was set to 0.0°C, triggering a false LOW temperature alarm. **Solution:** Send `NAN` on failure and check `isnan()` in AlarmTask before evaluating.

### 3. Encoder Race Condition
Reading and resetting the encoder position without synchronization lost ticks. **Solution:** Wrapped `Encoder_GetDelta()` in `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`.

### 4. Wokwi Simulation Limitation
Wokwi's STM32 simulation supports GPIO (LED blinks correctly) but does not implement UART serial output or OLED display for STM32Cube HAL framework. **Solution:** Verified correctness through unit tests (33/33 pass) and GPIO execution. Documented limitation in lab report.

### 5. FreeRTOS Stack Sizing
Initial task stacks caused occasional overflows. **Solution:** Enabled `configCHECK_FOR_STACK_OVERFLOW = 2`, added `vApplicationStackOverflowHook` with diagnostic output, and sized all stacks with 2x safety margins.

---

## Future Improvements

- **Wi-Fi connectivity** — Add ESP8266 for cloud logging via MQTT
- **Data logging** — SD card or flash-based timestamped data storage
- **Web dashboard** — Lightweight web interface for real-time visualization
- **Battery power** — Deep sleep modes with wake-on-motion
- **Additional sensors** — CO₂, air quality (MQ-135), barometric pressure
- **OTA updates** — Over-the-air firmware updates via BLE/Wi-Fi

---

## Resources

- **GitHub Repository:** [github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)
- **PlatformIO Documentation:** [platformio.org](https://platformio.org/)
- **FreeRTOS Reference:** [freertos.org](https://www.freertos.org/)
- **STM32F103 Datasheet:** [STMicroelectronics](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)

---

*Built as part of BCA182 Embedded Systems Programming — Mindanao State University - Iligan Institute of Technology*
