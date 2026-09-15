# BCA182 Room Monitoring System

## Overview
An environmental monitoring system built on an STM32 Blue Pill using FreeRTOS and PlatformIO. This project monitors temperature, humidity, light levels, and motion, displaying data on an SSD1306 OLED display with a rotary encoder for navigation.

## Features
- **Multi-sensor monitoring**: DHT22 (temperature/humidity), LDR (light), PIR (motion)
- **OLED display**: Real-time data visualization with page navigation
- **Rotary encoder**: Intuitive page switching between sensor readings
- **Alarm system**: Configurable temperature alerts with buzzer
- **Power management**: Auto-sleep when no motion detected
- **FreeRTOS**: Multi-task architecture with proper synchronization

## Hardware Requirements
- STM32 Blue Pill (STM32F103C8T6)
- DHT22 Temperature/Humidity Sensor
- LDR Photoresistor Module
- PIR Motion Sensor (HC-SR501)
- SSD1306 0.96" OLED Display (I2C)
- KY-040 Rotary Encoder
- Passive Buzzer
- ST-Link V2 Programmer

## Pin Connections
| Component | STM32 Pin | Protocol |
|-----------|-----------|----------|
| DHT22 | PA1 | Digital |
| LDR | PA0 | ADC |
| PIR | PB0 | Digital (EXTI) |
| OLED SCL | PB6 | I2C |
| OLED SDA | PB7 | I2C |
| Encoder CLK | PA2 | Digital (EXTI) |
| Encoder DT | PA3 | Digital |
| Encoder SW | PA4 | Digital (EXTI) |
| Buzzer | PB8 | PWM |

## Project Structure
```
BCA182-RoomMonitor/
├── src/
│   ├── main.c                    # Entry point
│   ├── main.h                    # Common definitions
│   ├── FreeRTOSConfig.h          # FreeRTOS configuration
│   ├── stm32f1xx_hal_conf.h      # HAL configuration
│   ├── app/
│   │   ├── hal/                  # Hardware drivers
│   │   │   ├── dht22.c/.h
│   │   │   ├── ldr.c/.h
│   │   │   ├── pir.c/.h
│   │   │   ├── oled.c/.h
│   │   │   ├── encoder.c/.h
│   │   │   └── buzzer.c/.h
│   │   ├── tasks/                # FreeRTOS tasks
│   │   │   ├── input_task.c/.h
│   │   │   ├── motion_task.c/.h
│   │   │   ├── sensor_task.c/.h
│   │   │   ├── alarm_task.c/.h
│   │   │   └── display_task.c/.h
│   │   └── logic/                # Application logic
│   │       ├── state_machine.c/.h
│   │       ├── temperature.c/.h
│   │       └── alarm.c/.h
│   └── drivers/
│       └── uart_mutex.c/.h       # UART mutex wrapper
├── test/
│   ├── test_temperature/
│   ├── test_encoder/
│   └── test_state_machine/
├── platformio.ini
├── wokwi.toml
└── diagram.json
```

## Building & Testing

### Build Firmware
```bash
pio run
```

### Upload to Board
```bash
pio run --target upload
```

### Run Unit Tests
```bash
pio test
```

### Static Analysis
```bash
pio check
```

## FreeRTOS Tasks

| Task | Priority | Description |
|------|----------|-------------|
| InputTask | 3 (High) | Handles rotary encoder input |
| MotionTask | 3 (High) | Processes PIR motion events |
| SensorTask | 2 (Mid) | Reads DHT22 and LDR sensors |
| AlarmTask | 2 (Mid) | Manages temperature alarms |
| DisplayTask | 1 (Low) | Updates OLED display |

## Unit Tests
- **Temperature tests**: 15 tests covering boundary conditions
- **Encoder tests**: 10 tests for navigation logic
- **State machine tests**: 8 tests for power management

## License
MIT License
