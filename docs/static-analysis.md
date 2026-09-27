# Static Analysis Report — BCA182 Room Monitoring System

**Tools:** LLVM clang-tidy 22.1.8, clang static analyzer (LLVM 22.1.8), `clang -Wall -Wextra`  
**Target:** STM32F103C8T6 + FreeRTOS application sources  
**Scope:** all 16 `.c` files under `src/` — `src/app/hal/` (6), `src/app/logic/` (3), `src/app/tasks/` (5), `src/drivers/` (1), plus `src/main.c`.  
**Date:** September 2026  
**Codebase:** STM32F103C8T6 + FreeRTOS HAL application

---

## Summary

| Severity | Count | Status |
|----------|-------|--------|
| HIGH | 0 | None |
| MEDIUM | 0 | None |
| LOW | 22 | Style / defensive-coding advisories — no corrective action required |

**Total findings:** 22  
**Functional defects:** 0

Analysis was performed in three passes. The **clang static analyzer** produced exactly one finding — `core.FixedAddressDereference` on the CMSIS `CoreDebug` register block in `src/app/hal/dht22.c:51`. That is a direct access to a memory-mapped peripheral register, which is how embedded code addresses hardware; it is reported here for completeness and is **not** a defect. Every remaining finding comes from the lower-severity `bugprone-*` and `cert-*` advisory checks, which flag stylistic and defensive-coding patterns rather than defects.

**Reproduce this yourself:** `tools/verify/run_static_analysis.sh` re-runs all three passes and prints the counts. See `tools/verify/README.md`.

---

## Analysis Method

The firmware targets an STM32F103 and normally builds under PlatformIO (`pio run -e bluepill_f103c8`), which is unavailable in the analysis environment. To analyse the application logic without the vendor SDK, a minimal stub header set was provided for the externally-supplied APIs (`stm32f1xx_hal.h`, `FreeRTOS.h`, `task.h`, `queue.h`, `semphr.h`, `event_groups.h`) and the translation units were analysed individually:

```bash
INC="-nostdinc -I<stubs> -Isrc -Isrc/app -Isrc/app/hal -Isrc/app/logic -Isrc/app/tasks -Iinclude"

# Pass 1 — deep flow-sensitive defect detection
for f in $(find src/app src/drivers -name '*.c'); do
    clang --analyze -std=c99 $INC "$f" -o /dev/null
done

# Pass 2 — advisory checks
for f in $(find src/app src/drivers -name '*.c'); do
    clang-tidy "$f" --checks='clang-analyzer-*,bugprone-*,cert-*' \
        -- $INC -std=c99 -fsyntax-only
done

# Pass 3 — compiler warnings
for f in $(find src/app src/drivers -name '*.c'); do
    clang -std=c99 -fsyntax-only -Wall -Wextra $INC "$f"
done
```

Only findings located within project sources (`src/`) are counted; findings arising from the stub headers themselves were excluded (the stub's own `__aeabi_*` and register helpers would otherwise add 14 `cert-dcl37-c` naming advisories that have nothing to do with the application). `-Wunused-function` is suppressed for the compiler pass, because replacing the HAL with stubs makes every callback that the real vector table would call appear unreferenced.

---

## Findings by Category

### 1. Narrowing Conversions — 10 findings

All 10 occur inside `OLED_DrawLine()` in `src/app/hal/oled.c` (lines 112–132), where the Bresenham accumulator mixes `int` with `int16_t`.

| Lines | Message |
|-------|---------|
| 112, 113, 116, 125, 127, 128, 131, 132 | `narrowing conversion from 'int' to signed type 'int16_t' is implementation-defined` |
| 112, 113 | (second occurrence per line) |

**Interpretation:** These are **advisory, not defects.** `int` → `int16_t` conversion is implementation-defined only when the `int` value falls outside the representable `int16_t` range. The accumulators hold pixel coordinates bounded by `OLED_WIDTH` (128) and `OLED_HEIGHT` (64), so the conversion never overflows in practice.

**Status:** Accepted. Making the conversion explicit (`(int16_t)` casts) would document intent but change no behaviour.

---

### 2. Easily-Swappable Parameters — 7 findings

| Line | Function | Message |
|------|----------|---------|
| `oled.c:76` | `OLED_SetPixel` | 2 adjacent `uint8_t` parameters easily swapped |
| `oled.c:86` | `OLED_DrawChar` | 2 adjacent parameters of convertible types easily swapped |
| `oled.c:111` | `OLED_DrawLine` | 2 adjacent `uint8_t` parameters easily swapped (×2) |
| `oled.c:144` | `OLED_FillRect` | 2 and 3 adjacent `uint8_t` parameters easily swapped |
| `oled.c:152` | `OLED_DrawProgressBar` | 2 adjacent `uint8_t` parameters easily swapped |

**Interpretation:** A **readability advisory shared by virtually every graphics API.** Signatures such as `OLED_FillRect(oled, x, y, w, h, color)` are conventional and self-documenting at every call site, which are all within `src/app/tasks/display_task.c`.

**Status:** Accepted. Introducing wrapper structs (`Point`, `Rect`) would be the analyzer's implied remedy but would obscure the drawing code.

---

### 3. Unchecked Return Values — 4 findings

All three occur in `src/app/tasks/display_task.c` at lines 19, 32, and 43 — the `snprintf()` calls that format the temperature, humidity, and light-level strings into a local `char buf[32]`.

| Location | Message |
|----------|---------|
| `display_task.c:19` | return value of `snprintf` disregarded (`cert-err33-c`) |
| `display_task.c:32` | return value of `snprintf` disregarded (`cert-err33-c`) |
| `display_task.c:43` | return value of `snprintf` disregarded (`cert-err33-c`) |
| `uart_mutex.c:15` | return value of `vsnprintf` disregarded (`cert-err33-c`) |

**Interpretation:** `snprintf()` and `vsnprintf()` truncate rather than overflow, so neither buffer can be overrun. The three `display_task.c` format strings (`"%.1f C"`, `"%.1f %%"`, `"%d"`) write at most a few bytes into a `char buf[32]`. The fourth, in `UART_Mutex_Printf()`, formats into `char buffer[256]`; the only call sites pass short literal prefixes such as `"[FATAL] Stack overflow in task: %s"` with a task name of at most `configMAX_TASK_NAME_LEN` characters, so the 256-byte bound is never approached.

**Status:** Accepted, with the following caveat recorded for completeness: the return value *would* matter if a format string could grow. A defensive `(void)` cast documents the deliberate discard. Not corrected, because the maximum output length is bounded by the format strings and the value ranges.

---

### 4. Compiler Warnings (`-Wall -Wextra`) — 1 finding

| Location | Message |
|----------|---------|
| `src/app/hal/oled.c:49` | `comparison of integers of different signs: 'int' and 'size_t'` |

`OLED_Clear()` iterates with `for (int i = 0; i < sizeof(oled->buffer); i++)` comparing a signed loop counter against the unsigned `sizeof`. The buffer is 1,024 bytes (`OLED_WIDTH * OLED_HEIGHT / 8` = 128 × 64 / 8), far below `INT_MAX`.

**Status:** Accepted. Strictly, the loop counter should be `size_t`; the signed type is safe here but the warning is legitimate.

---

## Categories That Produced No Findings

For completeness, the following were checked and produced **nothing** in this codebase — several are commonly assumed to be present in FreeRTOS projects:

| Category | Findings | Note |
|----------|----------|------|
| Unused parameters in FreeRTOS callbacks | 0 | The `bugprone-*` / `cert-*` check set does not include `-Wunused-parameter`; those warnings are only emitted by `-Wextra` and none were reported |
| Include order | 0 | `clang-tidy` `llvm-header-guard` is not part of the selected check set |
| Naming conventions | 0 | Not covered by the selected check set |
| Magic numbers | 0 | Not covered by the selected check set |
| Macro multiple-evaluation | 0 | The codebase defines no function-like macros of this kind |
| Dead stores | 0 | Reported by the static analyzer pass, which found nothing |

---

## Verdict

| Pass | Findings | Functional defects |
|------|----------|-------------------|
| clang static analyzer | 1 | 0 |
| clang-tidy (`bugprone-*`, `cert-*`) | 21 | 0 |
| `clang -Wall -Wextra` | 1 | 0 |

**No functional defect was detected.** No data race, null dereference, buffer overflow, memory leak, uninitialised read, or dead store was reported. The single analyzer finding is the benign memory-mapped register access described above. The 21 advisory findings are confined to `src/app/hal/oled.c` (10 narrowing conversions + 7 easily-swappable parameters = 17), `src/app/tasks/display_task.c` (3 deliberately discarded `snprintf` results), and `src/drivers/uart_mutex.c` (1 deliberately discarded `vsnprintf` result).

The `platformio.ini` `build_flags` do not currently enable `-Wall -Wextra`; the warning above was obtained by running the compiler with those flags explicitly and is recorded here for transparency. No remediation is required for correctness.
