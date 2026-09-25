# Real-Time Multisensor Room Monitoring System with FreeRTOS

## Project Overview

A real-time room-environment monitor built on the **STM32 Blue Pill**
(STM32F103C8T6) with **native FreeRTOS** and the **STM32Cube HAL**, developed in
**PlatformIO** and simulated in **Wokwi**. It reads temperature, humidity,
ambient light and motion, shows one measurement at a time on an SSD1306 OLED
selected with a rotary encoder, and sounds a buzzer when temperature leaves the
18–30 °C safe range. An ACTIVE/INACTIVE state machine blanks the display after
15 s without motion and restores it on the next motion event.

**GitHub Repository:** [github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)

---

## Motivation

Concurrency is where embedded systems get interesting — and where they break.
This project was an opportunity to design a real multi-task application
end-to-end: choose priorities by latency, pick the right synchronization
primitive for each interaction, and keep decision logic testable off-target. It
is also a demonstration that a serious FreeRTOS application fits comfortably on
an inexpensive Cortex-M3 with 20 KB of RAM.

---

## Features

- Multi-sensor monitoring: temperature, humidity, light, motion
- Six concurrent FreeRTOS tasks with justified priorities
- Rotary-encoder navigation across four OLED pages
- Temperature alarm with distinct low/high buzzer tones
- ACTIVE/INACTIVE state machine with a 15-second inactivity timeout
- Queues, a mutex, an event group and task notifications
- 33 host unit tests that link the real decision logic
- `pio check` clean at HIGH/MEDIUM severity

---

## Components

| Component | Purpose | Quantity |
|-----------|---------|----------|
| STM32 Blue Pill (STM32F103C8T6) | Main microcontroller | 1 |
| DHT22 | Temperature and humidity | 1 |
| Photoresistor (LDR) | Ambient light | 1 |
| PIR (HC-SR501) | Motion detection | 1 |
| SSD1306 OLED (128×64, I²C) | Display | 1 |
| KY-040 rotary encoder | Navigation input | 1 |
| Buzzer | Alarm output | 1 |

---

## Circuit

| Component | STM32 pin | Interface |
|-----------|-----------|-----------|
| DHT22 data | PA1 | 1-wire |
| LDR | PA0 | ADC1_CH0 |
| PIR | PB0 | EXTI0 |
| OLED SCL / SDA | PB6 / PB7 | I2C1 |
| Encoder CLK / DT / SW | PA2 / PA3 / PA4 | EXTI2 / GPIO / EXTI4 |
| Buzzer | PB8 | TIM4_CH3 PWM |
| USART1 | PA9 / PA10 | 115200 8N1 |

The Wokwi circuit is defined in `diagram.json`; `wokwi.toml` loads the real
`firmware.elf` built by PlatformIO.

---

## System Architecture

```
                 ┌───────────────────────────────────┐
                 │     STM32 Blue Pill (FreeRTOS)     │
                 └───────────────┬───────────────────┘
     ┌───────────────┬───────────┼───────────┬───────────────┐
     ▼               ▼           ▼           ▼               ▼
 SensorTask     MotionTask    InputTask   StateTask      AlarmTask
 (prio 2)       (prio 3)      (prio 3)    (prio 2)       (prio 2)
 DHT22/LDR      PIR EXTI      Encoder     state machine  buzzer
     │               │          │           │               │
 alarm_queue     event_group  display_    event_group         │
 display_queue               page_queue      │               │
     │                          │           │               │
     ▼                          ▼           ▼               ▼
                              DisplayTask (prio 1) ──► OLED
```

---

## FreeRTOS Architecture

| Task | Responsibility | Trigger / Period | Priority | IPC | Blocked on |
|------|----------------|------------------|----------|-----|------------|
| SensorTask | Read DHT22 + LDR | 1 s periodic | 2 | 2 queues | `vTaskDelayUntil()` |
| AlarmTask | Alarm + buzzer | sensor update | 2 | queue | `xQueueReceive()` |
| StateTask | ACTIVE/INACTIVE | 250 ms / motion | 2 | event group | `xEventGroupWaitBits()` |
| InputTask | Encoder navigation | encoder EXTI | 3 | notification + queue | `ulTaskNotifyTake()` |
| MotionTask | PIR | PIR EXTI / 500 ms | 3 | event group | `ulTaskNotifyTake()` |
| DisplayTask | OLED owner | 100 ms refresh | 1 | queues + event group | `xQueueReceive()` |

**Why these priorities.** The event-driven input tasks need the lowest latency,
so they run highest. The periodic/decision tasks run in the middle so fresh data
and safety decisions are ready before rendering. The display is cosmetic and runs
lowest, so it can never starve the producers. This ordering is verified by a
deliberate fault experiment (raising DisplayTask to priority 4 degrades sampling
and alarm latency).

**Synchronization.** Two independent sensor queues prevent competing consumers
from stealing each other's samples; the page queue carries encoder selection; the
event group carries motion and ACTIVE-state bits; the mutex serializes UART
messages; task notifications wake InputTask and MotionTask directly from the ISR.

---

## How It Works

1. **Boot.** `main()` calls `app_main()`, which initializes the HAL, creates the
   RTOS objects and six tasks, and starts the scheduler.
2. **Acquire.** Every second, SensorTask reads DHT22 and LDR and publishes one
   sample to both the alarm and display queues.
3. **Decide.** AlarmTask maps temperature to LOW/NORMAL/HIGH and toggles the
   buzzer; a failed read is `NAN` and is ignored, so faults cannot cause a false
   alarm. StateTask tracks motion via the event group and drives the OLED state.
4. **Present.** DisplayTask renders the selected page and blanks the panel while
   INACTIVE.
5. **React.** The PIR and encoder EXTI handlers notify MotionTask and InputTask;
   motion restores ACTIVE, and encoder detents change the displayed page.

---

## Testing and Verification

### Unit tests (33)

| Suite | Tests | Focus |
|-------|-------|-------|
| `test_temperature` | 15 | Threshold boundaries and alarm predicate |
| `test_navigation` | 10 | Page transitions and wraparound |
| `test_state_machine` | 8 | ACTIVE↔INACTIVE and timeout |

Run with `pio test -e native`. The tests link `temperature.c`,
`state_machine.c` and `display_page.c` directly.

### Static analysis

`pio check -e bluepill_f103c8`: **0 HIGH, 0 MEDIUM**, LOW-severity style findings
only (mostly cross-file `unusedFunction` false positives and optional
`const`-correctness suggestions).

### Functional verification

Ten Wokwi functional tests (FT-01 … FT-10) cover sensor display, encoder
navigation, alarm activation/deactivation and the state machine; procedures are
in `docs/functional-verification.md`.

---

## Demonstration

The Wokwi simulation shows the OLED cycling through Temperature, Humidity, Light
and Motion pages as the encoder is rotated, the buzzer activating outside the
temperature band, and the panel blanking after 15 s of inactivity and returning
when motion is detected. (Add the finished-system screenshot here for the public
post.)

---

## Challenges Encountered

- **A queue with two consumers.** The first design fed both AlarmTask and
  DisplayTask from one queue; because FreeRTOS delivers each item to a single
  consumer, samples were silently lost. Fix: one queue per consumer.
- **DHT22 timing.** The one-wire protocol needs microsecond timing; using
  `HAL_Delay()` inside a critical section blocked the tick. Fix: DWT cycle-count
  delays.
- **False alarm on sensor failure.** A failed read originally produced 0 °C and
  tripped the low alarm. Fix: publish `NAN` and ignore it in the alarm.
- **Encoder tick loss.** Reading and clearing the position could race with the
  ISR. Fix: a critical section around the read-and-clear.

---

## Lessons Learned

- Every synchronization object needs a real producer and consumer; objects added
  "for the checklist" are dead weight.
- A task that never blocks will starve lower-priority tasks — blocking is a
  feature, not a pause.
- Separating pure logic from hardware is what makes an embedded system testable
  on the host.
- Priorities encode latency requirements, not importance.

---

## Limitations

- LDR values are raw ADC counts, not calibrated lux.
- SRAM headroom is tight on the STM32F103C8; the heap and stacks dominate usage.
- Wokwi's peripheral fidelity is approximate compared with physical hardware.

---

## Future Improvements

- Add Wi-Fi/BLE connectivity and remote logging.
- Persist timestamped data to flash or SD.
- Adaptive alarm thresholds and a snooze input.
- Additional sensors (pressure, air quality).

---

## GitHub Repository

[github.com/Joal0816/BCA182-RoomMonitor](https://github.com/Joal0816/BCA182-RoomMonitor)

## References

- FreeRTOS Documentation — https://www.freertos.org/Documentation.html
- STM32CubeF1 / HAL — https://www.st.com/en/embedded-software/stm32cubef1.html
- PlatformIO STM32Cube framework — https://docs.platformio.org/en/latest/frameworks/stm32cube.html
- Wokwi Documentation — https://docs.wokwi.com/

---

*Built as part of BCA182 Embedded Systems Programming — Mindanao State University – Iligan Institute of Technology. Course instructor: Asst. Prof. Paul Rodolf P. Castor, M.Sc.*
