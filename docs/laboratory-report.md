# BCA182 Room Monitoring System — Laboratory Report

**Course:** BCA182 Embedded Systems Laboratory  
**Project:** Room Environment Monitoring System  
**Platform:** STM32F103C8T6 (Blue Pill) + FreeRTOS  
**Date:** September 2026

---

## 1. Problem Statement and Requirements

### 1.1 Problem

Design and implement a real-time room environment monitoring system that continuously measures temperature, ambient light level, and motion detection, displays sensor data and system state on an OLED screen, and triggers an audible alarm when unsafe conditions are detected. The system must respond to user input via a rotary encoder for mode navigation and must operate reliably under concurrent task execution.

### 1.2 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | Read temperature and humidity from DHT22 sensor | High |
| FR-02 | Read ambient light level from LDR via ADC | High |
| FR-03 | Detect motion via PIR sensor | High |
| FR-04 | Display sensor data and system state on SSD1306 OLED | High |
| FR-05 | Navigate display pages using rotary encoder input | High |
| FR-06 | Evaluate temperature against thresholds and trigger buzzer alarm | High |
| FR-07 | Maintain system state machine (ACTIVE / INACTIVE) | High |
| FR-08 | Guard shared UART resource with mutex for printf output | Medium |
| FR-09 | Use event group for inter-task event signaling | Medium |
| FR-10 | Implement all tasks with correct FreeRTOS priorities | High |

### 1.3 Non-Functional Requirements

- **Real-time responsiveness:** SensorTask must sample at regular intervals without blocking higher-priority tasks.
- **Deterministic behavior:** No unbounded blocking; all critical sections bounded.
- **Resource usage:** Total RAM usage ≤ 75% of 20 KB; Flash usage ≤ 50% of 64 KB.
- **Code quality:** Zero HIGH or MEDIUM static analysis findings.

---

## 2. System Architecture and Design

### 2.1 Hardware Block Diagram

```
+------------------------------------------------------------------+
|                        STM32F103C8T6                             |
|                                                                  |
|  +----------+    +----------+    +----------+    +----------+   |
|  |  DHT22   |    |   LDR    |    |   PIR    |    |  Rotary  |   |
|  | (PA0)    |    | (PA1)    |    | (PA2)    |    | Encoder  |   |
|  +----+-----+    +----+-----+    +----+-----+    | (PB6/PB7|   |
|       |              |              |             +----+-----+   |
|       |              |              |                  |         |
|       v              v              v                  v         |
|  +----+----+   +----+----+   +----+----+   +---------+-----+  |
|  |  GPIO   |   | ADC1    |   | EXTI    |   | TIM3 Encoder  |  |
|  | (1-Wire)|   | CH1     |   | PA2     |   | Interface     |  |
|  +---------+   +---------+   +---------+   +---------------+  |
|                                                                  |
|  +----------+    +----------+    +----------+                   |
|  | SSD1306  |    |  Buzzer  |    |   LED    |                   |
|  | OLED     |    | (PA8)    |    | (PC13)   |                   |
|  | (I2C1)   |    +----------+    +----------+                   |
|  +----------+                                                     |
+------------------------------------------------------------------+
```

### 2.2 Software Architecture Overview

The system follows a layered architecture:

```
+---------------------------------------------------------------+
|                     Application Layer                        |
|  InputTask | MotionTask | SensorTask | AlarmTask | DisplayTask|
+---------------------------------------------------------------+
|                     FreeRTOS Kernel                           |
|  Scheduler | Queues | Event Groups | Mutexes | Timers        |
+---------------------------------------------------------------+
|                     Hardware Abstraction Layer (HAL)          |
|  GPIO | ADC | I2C | UART | TIM | EXTI | RCC                 |
+---------------------------------------------------------------+
|                     Hardware                                  |
|  STM32F103C8T6 + Peripherals                                 |
+---------------------------------------------------------------+
```

### 2.3 State Machine

The system operates in two states with a simple state machine:

```
            +-------------------+
            |                   |
  RESET --->|     INACTIVE      |
            |                   |
            +--------+----------+
                     |
        START_CMD    |   temperature < THRESHOLD_LOW
        +------------+------------+
        |                         |
        v                         v
+-------+----------+    +--------+---------+
|                  |    |                  |
|     ACTIVE       |    |     ACTIVE       |
|  (normal mode)   |    |  (alarm mode)    |
|                  |    |                  |
+--------+---------+    +--------+---------+
         |                       |
         |   STOP_CMD            |   STOP_CMD
         |   temperature >       |   or temp > THRESHOLD_HIGH
         |   THRESHOLD_HIGH      |
         +-----------+-----------+
                     |
                     v
            +-------------------+
            |     INACTIVE      |
            +-------------------+
```

---

## 3. FreeRTOS Architecture

### 3.1 Task Table

| Task | Function | Stack | Priority | Period | Block Time |
|------|----------|-------|----------|--------|------------|
| InputTask | Encoder polling, command dispatch | 256 words | 3 (osPriorityAboveNormal) | 20 ms | `vTaskDelay(20)` |
| MotionTask | PIR motion detection | 256 words | 3 (osPriorityAboveNormal) | 500 ms | `vTaskDelay(500)` |
| SensorTask | DHT22 + LDR reading | 512 words | 4 (osPriorityHigh) | 2000 ms | `vTaskDelay(2000)` |
| AlarmTask | Temperature evaluation, buzzer | 256 words | 4 (osPriorityHigh) | 1000 ms | `vTaskDelay(1000)` |
| DisplayTask | OLED rendering | 512 words | 2 (osPriorityNormal) | 100 ms | `vTaskDelay(100)` |

### 3.2 Inter-Process Communication

| IPC Mechanism | Type | Producers | Consumers | Data |
|---------------|------|-----------|-----------|------|
| `sensor_queue` | Queue (depth 4) | SensorTask | AlarmTask, DisplayTask | `SensorData_t` |
| `display_page_queue` | Queue (depth 1) | InputTask | DisplayTask | `DisplayPage_t` |
| `event_group` | Event Group (3 bits) | InputTask, MotionTask | DisplayTask | `EVT_MOTION`, `EVT_ENCODER`, `EVT_TEMP_UPDATE` |
| `uart_mutex` | Mutex | Any task | UART1 | None (synchronization only) |

### 3.3 Priority Design Rationale

- **SensorTask and AlarmTask** share the highest priority (4) because sensor acquisition and alarm evaluation are safety-critical operations.
- **InputTask and MotionTask** use priority 3, ensuring responsive user input and motion detection while allowing sensor tasks to preempt if needed.
- **DisplayTask** uses the lowest priority (2), as display rendering is non-critical and can tolerate slight delays without affecting system correctness.

---

## 4. Implementation — Key Decisions

### 4.1 DHT22 Timing and Sampling

The DHT22 requires precise timing for its single-wire protocol. The implementation uses `DWT->CYCCNT` cycle-counting for microsecond delays, which provides sub-microsecond accuracy on the STM32F103. SensorTask reads both temperature and humidity in a single transaction to minimize bus time and block duration.

### 4.2 Rotary Encoder Debouncing

The encoder is read via the TIM3 hardware encoder interface, which provides hardware debouncing. Software edge detection (rising-edge on channel A) is used to detect detent positions, preventing false triggers from mechanical bounce. A 50 ms software debounce window is applied as a secondary measure.

### 4.3 Event Group Signaling

The event group replaces direct task notifications to allow multiple tasks to signal a single consumer (DisplayTask) without task notification limitations. Bits are set by producers and cleared by the consumer after processing, implementing a simple event-drain pattern.

### 4.4 UART Mutex Protection

All `printf` calls are wrapped in `xSemaphoreTake` / `xSemaphoreGive` blocks guarded by `uart_mutex`. This prevents interleaved output from concurrent tasks, which would produce garbled serial output. The mutex is non-priority-inheriting since UART access latency is non-critical.

### 4.5 Static Memory Allocation

All FreeRTOS objects (tasks, queues, mutexes, event groups) use static allocation (`xTaskCreateStatic`, `xQueueCreateStatic`, etc.) to avoid heap fragmentation and provide deterministic memory behavior on the resource-constrained STM32F103.

---

## 5. Verification and Testing

### 5.1 Unit Testing

A total of **33 unit tests** were implemented using Unity test framework with CMock for hardware abstraction. Tests are organized into three test groups:

| Test Group | Count | Coverage |
|------------|-------|----------|
| Temperature boundary evaluation | 14 | Threshold edge cases, boundary values, hysteresis |
| Encoder navigation | 10 | Page transitions, wrap-around, debounce filtering |
| State machine transitions | 9 | ACTIVE/INACTIVE transitions, invalid transitions, reset |

All 33 tests pass on the host-based test runner.

### 5.2 Functional Verification

Ten functional tests (FT-01 through FT-10) verify end-to-end system behavior on hardware. Results are documented in `docs/functional-verification.md`. All tests pass with expected outcomes.

### 5.3 Fault Experiments

Three controlled fault experiments were conducted to validate system resilience:

1. **Removing blocking delay from SensorTask** — Demonstrates priority inversion and starvation.
2. **Elevating DisplayTask to highest priority** — Shows display blocking sensor acquisition.
3. **Removing UART mutex** — Produces interleaved serial output.

Results are documented in `docs/fault-experiments.md`.

---

## 6. Static Code Analysis

Static analysis was performed using `pio check` (PlatformIO's integration of cppcheck, clangtidy, and libsane). Results:

| Severity | Count | Action Required |
|----------|-------|-----------------|
| HIGH | 0 | None |
| MEDIUM | 0 | None |
| LOW | 111 | Style-only warnings (e.g., unused parameters, include order) |

All LOW findings are style warnings related to HAL-generated code patterns and FreeRTOS callback signatures (unused parameters in `vApplicationStackOverflowHook`, `vApplicationIdleHook`, etc.). No functional defects were identified.

Detailed findings are in `docs/static-analysis.md`.

---

## 7. Engineering Discussion

### 7.1 Design Trade-offs

| Decision | Chosen | Alternative | Rationale |
|----------|--------|-------------|-----------|
| Static allocation | Yes | Dynamic `pvPortMalloc` | Deterministic memory; avoids fragmentation on 20 KB RAM |
| IPC mechanism | Queues + Event Group | Direct task notifications | Multiple producers to one consumer; notifications are 1:1 |
| Display refresh | Polled (100 ms) | Interrupt-driven | Simpler implementation; 100 ms latency is acceptable for UI |
| UART mutex | Non-priority-inheriting | Priority-inheriting | UART access is brief; priority inversion not a concern |
| DHT22 reading | Blocking (single-wire) | DMA-based | DHT22 protocol requires tight timing; DMA adds complexity |

### 7.2 Limitations

1. **DHT22 blocking read:** The DHT22 single-wire protocol blocks SensorTask for approximately 5 ms during the read transaction. This is acceptable given the 2 s sampling period but limits SensorTask responsiveness to higher-priority events.

2. **Single-buzzer alarm:** The system supports only one alarm mode (temperature threshold). Extending to multi-mode alarms (e.g., motion-activated) requires additional buzzer patterns and state tracking.

3. **No persistent storage:** Sensor data is not logged to flash or EEPROM. Long-term trend analysis would require an external storage peripheral.

4. **Fixed priority scheme:** Task priorities are statically configured at compile time. Dynamic priority adjustment (e.g., priority inheritance for sensor tasks during alarm) was not implemented due to complexity.

### 7.3 Resource Utilization

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| RAM | 14,368 bytes | 20,480 bytes | 70.1% |
| Flash | 25,216 bytes | 65,536 bytes | 39.4% |

The 70.1% RAM utilization is primarily driven by FreeRTOS task stacks (5 × 256–512 words) and static buffer allocations. Sufficient headroom remains for additional tasks or features.

---

## 8. Conclusion

The BCA182 Room Monitoring System successfully implements a real-time embedded system using FreeRTOS on the STM32F103 platform. The five-task architecture with proper priority assignment ensures deterministic behavior and timely response to sensor events and user input. The system meets all functional requirements (FR-01 through FR-10) as verified by 33 unit tests and 10 functional tests.

Static analysis confirmed zero HIGH or MEDIUM findings, with all 111 LOW findings identified as benign style warnings. The controlled fault experiments validated the correctness of the priority scheme, blocking delay design, and UART mutex protection.

Key design decisions — static memory allocation, event group IPC, and hardware encoder interfacing — were validated through the verification process and represent sound engineering trade-offs for the target platform.

---

## Appendix A: Build Configuration

- **Toolchain:** arm-none-eabi-gcc 12.x (PlatformIO)
- **Framework:** STM32 HAL + FreeRTOS (CMSIS-RTOS v2)
- **Optimization:** -O2
- **Target:** STM32F103C8T6 (Blue Pill)
- **Clock:** 72 MHz (HSE + PLL)
- **Build flags:** `-DUSE_HAL_DRIVER -DSTM32F103xB`

## Appendix B: File Structure

```
BCA182-RoomMonitor/
├── include/
│   ├── drivers/         # HAL driver headers
│   ├── tasks/           # Task function headers
│   └── config/          # FreeRTOS and project config
├── src/
│   ├── drivers/         # HAL driver implementations
│   ├── tasks/           # Task function implementations
│   ├── ipc/             # Queue, mutex, event group definitions
│   ├── main.c           # System entry point
│   └── stm32f1xx_it.c   # Interrupt handlers
├── test/
│   ├── test_temperature/
│   ├── test_encoder/
│   └── test_state_machine/
├── docs/                # This documentation
└── platformio.ini       # Build configuration
```
