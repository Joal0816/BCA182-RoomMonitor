# Real-Time Multisensor Room Monitoring System with FreeRTOS

![PlatformIO](https://img.shields.io/badge/PlatformIO-IDE-orange) ![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Kernel-green) ![STM32](https://img.shields.io/badge/STM32-BluePill-blue) ![License](https://img.shields.io/badge/License-MIT-yellow)

## Project Overview

This project implements a real-time multisensor room monitoring system built on the STM32 Blue Pill (STM32F103C8T6) using FreeRTOS and PlatformIO. The system continuously monitors temperature, humidity, light levels, and motion using a DHT22, LDR, and PIR sensor, with data displayed on an SSD1306 OLED and alarms triggered via a buzzer. A rotary encoder provides intuitive menu navigation, and an ACTIVE/INACTIVE state machine conserves power when the room is unoccupied.

## Motivation

Room monitoring systems are a cornerstone of IoT and smart building projects, yet most tutorials oversimplify the implementation by relying on bare-metal polling loops. This project demonstrates how to build a production-quality embedded system using a real-time operating system (RTOS), proper task scheduling, state machines, and modular software architecture — skills directly transferable to industrial and commercial applications. By documenting the design decisions, testing strategies, and lessons learned, this project serves as a practical reference for anyone learning embedded systems development with FreeRTOS on ARM Cortex-M microcontrollers.

## Features

- **Multisensor monitoring** — Simultaneous acquisition of temperature, humidity, light intensity, and motion data
- **Real-time task scheduling** — 5 FreeRTOS tasks with defined priorities for deterministic behavior
- **State machine control** — ACTIVE/INACTIVE modes with a 15-second inactivity timeout
- **Intuitive UI navigation** — Rotary encoder for menu browsing and parameter adjustment
- **Visual feedback** — SSD1306 OLED display showing sensor readings and system status
- **Audible alarms** — Buzzer notifications for threshold violations and motion detection
- **Modular architecture** — Clean separation of drivers, middleware, and application logic
- **Comprehensive testing** — 33 unit tests with 100% pass rate
- **Static analysis verified** — Zero HIGH/MEDIUM findings from cppcheck and clang-tidy
- **Low power potential** — Inactive state reduces unnecessary processing

## Components Used

| Component | Purpose | Quantity |
|---|---|---|
| STM32 Blue Pill (STM32F103C8T6) | Main microcontroller | 1 |
| DHT22 (AM2302) | Temperature and humidity sensor | 1 |
| LDR (GL5528) | Light level detection | 1 |
| PIR Sensor (HC-SR501) | Motion detection | 1 |
| SSD1306 OLED (0.96", I2C) | Visual display output | 1 |
| Active Buzzer Module | Audible alarm output | 1 |
| Rotary Encoder with push button | Menu navigation input | 1 |
| 10kΩ Resistor | LDR voltage divider | 1 |
| Breadboard | Prototyping | 1 |
| Jumper wires | Connections | ~20 |
| USB-to-Serial adapter | Programming/debugging | 1 |

## Circuit Design

### Power Supply
- STM32 Blue Pill powered via USB (5V) with onboard 3.3V regulator
- All sensors powered from the 3.3V rail (PIR can accept 5V but logic is 3.3V compatible)

### Sensor Connections

| Signal | STM32 Pin | Notes |
|---|---|---|
| DHT22 Data | PA0 | 10kΩ pull-up to 3.3V |
| LDR (analog) | PA1 (ADC1_CH1) | Part of voltage divider with 10kΩ to GND |
| PIR Output | PA2 | Digital input, 3.3V logic |
| OLED SDA | PB7 (I2C1_SDA) | 4.7kΩ pull-up to 3.3V |
| OLED SCL | PB6 (I2C1_SCL) | 4.7kΩ pull-up to 3.3V |
| Buzzer Signal | PA8 | Via NPN transistor (2N2222) base resistor 1kΩ |
| Encoder A | PB12 | Internal pull-up enabled |
| Encoder B | PB13 | Internal pull-up enabled |
| Encoder Button | PB14 | Internal pull-up enabled |

### Design Considerations
- **I2C pull-ups**: 4.7kΩ resistors ensure reliable communication at 400kHz
- **LDR voltage divider**: Provides a voltage swing across the ADC range for accurate light readings
- **Buzzer driver**: Transistor switches the buzzer to avoid drawing excessive current from the GPIO pin
- **Debouncing**: Encoder inputs use software debouncing with 20ms timeout intervals

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATION LAYER                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │  Sensor Mgr  │  │   Display    │  │    Alarm     │              │
│  │  (Collection │  │   Manager    │  │   Manager    │              │
│  │  & Filtering)│  │  (UI Render) │  │  (Threshold) │              │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘              │
│         │                 │                 │                        │
│         └─────────────────┼─────────────────┘                        │
│                           │                                         │
│  ┌────────────────────────┴──────────────────────────────────────┐  │
│  │                  STATE MACHINE (ACTIVE/INACTIVE)              │  │
│  │            15s timeout → INACTIVE → motion → ACTIVE           │  │
│  └───────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│                        MIDDLEWARE LAYER                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │   FreeRTOS   │  │   Message    │  │   Encoder    │              │
│  │   Scheduler  │  │   Queues     │  │   Handler    │              │
│  └──────────────┘  └──────────────┘  └──────────────┘              │
├─────────────────────────────────────────────────────────────────────┤
│                        DRIVER LAYER                                  │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  DHT22  │ │  ADC    │ │  GPIO   │ │  I2C    │ │  Timer  │      │
│  │  Driver │ │  Driver │ │  Driver │ │  Driver │ │  Driver │      │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘      │
├─────────────────────────────────────────────────────────────────────┤
│                    HARDWARE ABSTRACTION (HAL/CMSIS)                 │
├─────────────────────────────────────────────────────────────────────┤
│                    STM32F103C8T6 (ARM Cortex-M3)                    │
└─────────────────────────────────────────────────────────────────────┘
```

## FreeRTOS Architecture

The system runs 5 tasks with carefully chosen priorities to ensure real-time responsiveness:

| Task | Function | Priority | Stack | Period |
|---|---|---|---|---|
| `Task_SensorRead` | Reads DHT22, LDR (ADC), PIR (GPIO) | 3 (High) | 256 words | 2000ms |
| `Task_DisplayUpdate` | Renders sensor data on SSD1306 OLED | 2 (Medium) | 256 words | 500ms |
| `Task_EncoderHandler` | Processes rotary encoder input + debounce | 2 (Medium) | 128 words | Event-driven |
| `Task_AlarmManager` | Evaluates thresholds, triggers buzzer | 4 (Highest) | 128 words | 1000ms |
| `Task_StateMachine` | Manages ACTIVE/INACTIVE transitions | 1 (Low) | 128 words | 1000ms |

### Inter-Task Communication

- **Sensor → Display**: FreeRTOS queue传递sensor readings
- **Encoder → StateMachine**: Queue传递navigation events
- **Alarm ↔ StateMachine**: Shared state with critical section protection

### Key Design Decisions

1. **Sensor task has highest non-critical priority** — ensures data freshness without starving the alarm
2. **Alarm task has absolute highest priority** — critical alerts must never be delayed
3. **State machine runs lowest priority** — background management doesn't block sensor/display
4. **Encoder is event-driven** — interrupts wake the task only when input occurs

## How It Works

### Step 1: System Initialization
On power-up, the HAL initializes clocks, GPIO, I2C, and ADC peripherals. FreeRTOS creates all 5 tasks and associated queues, then starts the scheduler.

### Step 2: Sensor Data Acquisition
The sensor task reads the DHT22 (temperature + humidity) via one-wire protocol, samples the LDR through the ADC (12-bit resolution), and checks the PIR digital output. Raw values are filtered using a simple moving average to reduce noise.

### Step 3: State Machine Evaluation
The state machine task checks the PIR sensor flag. If motion is detected, the system transitions to ACTIVE mode (or resets the inactivity timer if already active). After 15 seconds without motion, the system transitions to INACTIVE mode, reducing display updates and disabling non-essential alarms.

### Step 4: Display Rendering
In ACTIVE mode, the display task renders a multi-page UI:
- **Page 1**: Temperature and humidity with graphical indicators
- **Page 2**: Light level as a percentage bar
- **Page 3**: System status and uptime

The rotary encoder cycles between pages with a left/right turn.

### Step 5: Alarm Evaluation
The alarm task continuously monitors sensor thresholds:
- Temperature > 35°C → High temperature alarm
- Humidity > 80% → High humidity alarm
- PIR motion detected while INACTIVE → Wake-up alert

Thresholds are adjustable via the encoder in a settings menu.

## Software Design

The codebase follows a three-layer modular architecture:

```
BCA182-RoomMonitor/
├── src/
│   ├── app/                    # Application logic
│   │   ├── main.c              # Entry point, task creation
│   │   ├── sensor_manager.c    # Sensor coordination & filtering
│   │   ├── display_manager.c   # OLED rendering logic
│   │   ├── alarm_manager.c     # Threshold evaluation & buzzer
│   │   └── state_machine.c     # ACTIVE/INACTIVE state logic
│   ├── drivers/                # Hardware abstraction
│   │   ├── dht22.c             # DHT22 one-wire driver
│   │   ├── ssd1306.c           # SSD1306 I2C display driver
│   │   ├── adc.c               # ADC configuration & reading
│   │   ├── gpio.c              # GPIO input/output setup
│   │   └── encoder.c           # Rotary encoder handler
│   └── config/                 # System configuration
│       ├── freertos_config.h   # FreeRTOS kernel settings
│       └── app_config.h        # Pin mappings & thresholds
├── include/                    # Public headers
├── test/                       # Unit tests
│   ├── test_sensor_manager.c
│   ├── test_state_machine.c
│   ├── test_alarm_manager.c
│   └── test_display_manager.c
├── platformio.ini              # Build configuration
└── docs/                       # Documentation
```

### Design Principles
- **Separation of concerns**: Drivers know nothing about application logic
- **Testability**: Each module exposes a clean API for unit testing
- **Portability**: Drivers abstract HAL specifics behind consistent interfaces
- **Configuration-driven**: Pin assignments and thresholds in config headers

## Testing and Verification

### Unit Testing
The project includes **33 unit tests** implemented using the Unity test framework, executed via PlatformIO's test runner. Tests cover:

| Module | Test Cases | Coverage |
|---|---|---|
| Sensor Manager | 9 | Data validation, filtering, edge cases |
| State Machine | 8 | Transitions, timeout, reset conditions |
| Alarm Manager | 8 | Threshold evaluation, buzzer control |
| Display Manager | 8 | Page rendering, navigation logic |

All 33 tests pass on every build with zero warnings.

### Static Analysis
- **cppcheck**: Zero HIGH and MEDIUM severity findings
- **clang-tidy**: No critical or medium-level issues
- **Compiler warnings**: `-Wall -Wextra -Werror` enforced — zero warnings in release builds

### Continuous Integration
GitHub Actions workflow runs on every push:
1. Build for STM32F103C8T6 target
2. Execute all 33 unit tests
3. Run static analysis checks
4. Generate build artifact for firmware flashing

## Challenges and Lessons Learned

### 1. DHT22 Timing Sensitivity
The DHT22 uses a precise one-wire protocol with microsecond-level timing. FreeRTOS task preemption during the read sequence caused intermittent failures. **Solution**: Temporarily disable interrupts during the critical section of the DHT22 read, keeping the critical window under 5ms to avoid starving other tasks.

### 2. ADC Noise on LDR Readings
Raw ADC values fluctuated by ±20 counts due to power supply noise. **Solution**: Implemented a simple moving average filter with a window size of 8 samples, reducing noise to ±3 counts without introducing perceptible lag.

### 3. Encoder Debouncing
Mechanical bounce on the rotary encoder generated spurious navigation events. **Solution**: Implemented a 20ms software debounce timer with state tracking to filter transient signals while maintaining responsive input feel.

### 4. FreeRTOS Stack Overflow Detection
Early debugging revealed occasional hard faults traced to stack overflows. **Solution**: Enabled `configCHECK_FOR_STACK_OVERFLOW` in FreeRTOS config and added a stack watermark monitoring task during development. All task stacks were sized with 2x safety margins.

### 5. I2C Bus Contention
The OLED display occasionally froze when multiple tasks attempted I2C access simultaneously. **Solution**: Wrapped all I2C transactions in a FreeRTOS mutex, ensuring serialized bus access.

## Future Improvements

- **Wi-Fi connectivity**: Add an ESP8266 or ESP32 co-processor for cloud logging and remote monitoring via MQTT
- **Data logging**: Implement SD card or flash-based data logging with timestamped entries
- **Web dashboard**: Build a lightweight web interface for real-time data visualization
- **Battery power**: Implement deep sleep modes with wake-on-motion for portable deployment
- **Additional sensors**: Expand with CO₂, air quality (MQ-135), or barometric pressure sensors
- **OTA updates**: Add over-the-air firmware update capability via BLE or Wi-Fi
- **Machine learning**: On-device anomaly detection for occupancy pattern learning

## Resources

- **GitHub Repository**: [Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)
- **PlatformIO Documentation**: [platformio.org](https://platformio.org/)
- **FreeRTOS Reference**: [freertos.org](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-Porting/01-Porting-a-FreeRTOS-kernel)
- **STM32F103 Datasheet**: [STMicroelectronics](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- **DHT22 Datasheet**: [Aosong AM2302](http://www.aosong.com/en/products-21.html)
- **SSD1306 Datasheet**: [Solomon Systech](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

---

*Built with ❤️ as part of BCA182 — demonstrating that professional-grade embedded systems can be built with accessible components and open-source tools.*
