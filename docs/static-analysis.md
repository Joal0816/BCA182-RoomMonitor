# Static Code Analysis Report — BCA182 Room Monitoring System

**Tool:** PlatformIO Check (`pio check -e bluepill_f103c8`)  
**Analyzer:** cppcheck  
**Command:** `pio check -e bluepill_f103c8`

---

## Summary

| Severity | Count |
|----------|-------|
| HIGH | 0 |
| MEDIUM | 0 |
| LOW | 76 |

`pio check` reports **no functional defects**. All findings are LOW-severity
style observations.

### Findings by component

| Component | HIGH | MEDIUM | LOW |
|-----------|------|--------|-----|
| `src` | 0 | 0 | 28 |
| `src/app/hal` | 0 | 0 | 24 |
| `src/app/logic` | 0 | 0 | 15 |
| `src/app/tasks` | 0 | 0 | 6 |
| `src/drivers` | 0 | 0 | 3 |
| **Total** | **0** | **0** | **76** |

---

## Findings by category

### 1. `unusedFunction` — cross-translation-unit false positives

cppcheck analyses each translation unit independently. Functions that are
defined in one file and called from another (for example every `Task` function
created in `main.c`, and every driver entry point) are reported as "never used"
even though the linker resolves and calls them.

Example findings:

| File | Function | Explanation |
|------|----------|-------------|
| `src/app/tasks/sensor_task.c` | `SensorTask` | Called from `main.c` via `xTaskCreate` (function pointer) |
| `src/app/tasks/state_task.c` | `StateTask` | Called from `main.c` via `xTaskCreate` |
| `src/app/hal/dht22.c` | `DHT22_Read` | Called from `sensor_task.c` |
| `src/app/logic/temperature.c` | `EvaluateTemperature` | Called from `alarm_task.c` and `display_task.c` |
| `src/main.c` | `EXTI0_IRQHandler`, `SysTick_Handler` | Referenced by the vector table |
| `src/main.c` | `vApplicationStackOverflowHook` | Referenced by the FreeRTOS kernel |
| `src/stm32f1xx_hal_msp.c` | `HAL_MspInit`, `HAL_*_MspInit` | Weak HAL callbacks invoked by `HAL_Init` / peripheral init |

**Interpretation:** these are analyser limitations, not dead code. The build
links successfully and the functions are reachable (task creation uses function
pointers, which the per-file analyser cannot follow).

### 2. `constParameterPointer` — const-correctness style

| File | Function | Status |
|------|----------|--------|
| `src/app/hal/encoder.c` | `Encoder_IsButtonPressed` | Remaining — could take `const Encoder_t *` |

The read-only accessors `DHT22_ComputeChecksum`, `LDR_GetValue`, `PIR_GetState`,
`Buzzer_IsPlaying`, `Alarm_IsActive` and `UART_Mutex_Send` were corrected to take
`const` parameters during this review; one suggestion remains.

**Interpretation:** an optional readability/const-correctness suggestion that does
not affect behaviour.

---

## Corrective actions

| Category | Action | Status |
|----------|--------|--------|
| `unusedFunction` (75) | No code change — verified reachable; documented as analyser limitation | Accepted |
| `constParameterPointer` (1) | Apply `const` to remaining read-only accessor (`Encoder_IsButtonPressed`) | Planned |

No HIGH or MEDIUM findings required remediation. The analysis confirms there are
no detected correctness or safety defects in the firmware.
