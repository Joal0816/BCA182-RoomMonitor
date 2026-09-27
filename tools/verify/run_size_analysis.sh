#!/usr/bin/env bash
#
# Cross-compile the application for Cortex-M3 using clang's built-in ARM target
# (no arm-none-eabi-gcc required) and report section sizes.
#
# IMPORTANT: this does NOT reproduce the figures in report section 7.1. Those
# came from a full PlatformIO build that links the real STM32Cube HAL, the real
# FreeRTOS kernel, and the startup/vector-table code -- several tens of KB of
# object code that is absent here. What this script measures is the size of the
# *application's own* code and static data, which is the part the student wrote.
#
# It also links a synthetic ELF (with HAL/FreeRTOS calls stubbed out and the
# 12 KB FreeRTOS heap included) to show the shape of the memory map.
#
# Usage: tools/verify/run_size_analysis.sh        (from the repository root)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STUB="$ROOT/tools/verify/stub"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cd "$ROOT"

INC=(-Isrc -Isrc/app -Isrc/app/hal -Isrc/app/logic -Isrc/app/tasks -Isrc/drivers)
CFLAGS=(-target arm-none-eabi -mcpu=cortex-m3 -mthumb -std=c99 -w -Os
        -nostdinc -I"$STUB" "${INC[@]}")

echo "=== Compiling src/ for Cortex-M3 (clang -target arm-none-eabi) ==="
ok=0 bad=0
for f in $(find src -name '*.c' | sort); do
    o="$BUILD/$(echo "$f" | tr '/' '_').o"
    if clang "${CFLAGS[@]}" -c "$f" -o "$o" 2>"$BUILD/err"; then
        ok=$((ok + 1))
    else
        echo "  FAIL $f"
        grep -m1 'error:' "$BUILD/err" | sed 's/^/       /'
        bad=$((bad + 1))
    fi
done
echo "compiled: $ok ok, $bad failed"
[ "$bad" -eq 0 ] || exit 1

echo
echo "=== Application section sizes (bytes) ==="
printf '%-40s %8s %9s %7s %7s\n' object .text .rodata .data .bss
python3 - "$BUILD" <<'PY'
import subprocess, glob, os, sys
build = sys.argv[1]
fields = ('.text', '.rodata', '.data', '.bss')
tot = dict.fromkeys(fields, 0)
for o in sorted(glob.glob(os.path.join(build, '*.o'))):
    out = subprocess.run(['size', '-A', o], capture_output=True, text=True).stdout
    d = {}
    for ln in out.splitlines():
        p = ln.split()
        if len(p) >= 2 and p[0] in fields:
            d[p[0]] = int(p[1])
    for k in fields:
        tot[k] += d.get(k, 0)
    print('%-40s %8d %9d %7d %7d' % (os.path.basename(o)[:-2],
          d.get('.text', 0), d.get('.rodata', 0), d.get('.data', 0), d.get('.bss', 0)))
print('-' * 73)
print('%-40s %8d %9d %7d %7d' % ('APPLICATION TOTAL',
      tot['.text'], tot['.rodata'], tot['.data'], tot['.bss']))
flash = tot['.text'] + tot['.rodata'] + tot['.data']
print()
print('Application flash footprint (.text+.rodata+.data) : %6d B' % flash)
print('Application static RAM   (.data+.bss)             : %6d B' % (tot['.data'] + tot['.bss']))
print('  (OLED statics: 1024 B framebuffer in main.c + 1025 B tx_buf in oled.c)')
PY

echo
echo "=== Linked ELF estimate (heap + stubs included) ==="
cat > "$BUILD/startup_stub.c" <<'EOF'
#include <stdint.h>
typedef struct { unsigned CTRL, CYCCNT; } DWT_Type;
uint32_t SystemCoreClock = 72000000U;
DWT_Type *DWT = (DWT_Type *)0xE0001000;
/* configTOTAL_HEAP_SIZE from src/FreeRTOSConfig.h */
uint8_t ucHeap[12 * 1024];
EOF
cat > "$BUILD/halstub.c" <<'EOF'
#include <stdint.h>
typedef struct { void *Instance; } G;
typedef struct { void *Instance; } AH, IH, TH, UH;
void HAL_Init(void) {}
void HAL_Delay(uint32_t m) { (void)m; }
uint32_t HAL_GetTick(void) { return 0; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return 72000000U; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 36000000U; }
void *HAL_RCC_OscConfig(void *a) { (void)a; return 0; }
void *HAL_RCC_ClockConfig(void *a, uint32_t b) { (void)a; (void)b; return 0; }
void HAL_GPIO_Init(G *p, void *c) { (void)p; (void)c; }
int  HAL_GPIO_ReadPin(G *p, uint16_t n) { (void)p; (void)n; return 0; }
void HAL_GPIO_TogglePin(G *p, uint16_t n) { (void)p; (void)n; }
void HAL_GPIO_WritePin(G *p, uint16_t n, int s) { (void)p; (void)n; (void)s; }
void HAL_NVIC_SetPriority(int i, uint32_t a, uint32_t b) { (void)i; (void)a; (void)b; }
void HAL_NVIC_EnableIRQ(int i) { (void)i; }
void *HAL_ADC_ConfigChannel(AH *h, void *c) { (void)h; (void)c; return 0; }
uint32_t HAL_ADC_GetValue(AH *h) { (void)h; return 0; }
void *HAL_ADC_PollForConversion(AH *h, uint32_t t) { (void)h; (void)t; return 0; }
void *HAL_ADC_Start(AH *h) { (void)h; return 0; }
void *HAL_ADC_Stop(AH *h) { (void)h; return 0; }
void *HAL_TIM_PWM_Start(TH *h, uint32_t c) { (void)h; (void)c; return 0; }
void *HAL_TIM_PWM_Stop(TH *h, uint32_t c) { (void)h; (void)c; return 0; }
void *HAL_I2C_Master_Transmit(IH *h, uint16_t a, uint8_t *d, uint16_t n, uint32_t t)
    { (void)h; (void)a; (void)d; (void)n; (void)t; return 0; }
void *HAL_UART_Transmit(UH *h, uint8_t *d, uint16_t n, uint32_t t)
    { (void)h; (void)d; (void)n; (void)t; return 0; }
void __aeabi_memclr4(void *a, unsigned n) { (void)a; (void)n; }
void __aeabi_memclr(void *a, unsigned n) { (void)a; (void)n; }
void __aeabi_memcpy4(void *a, const void *b, unsigned n) { (void)a; (void)b; (void)n; }
EOF
clang "${CFLAGS[@]}" -c "$BUILD/startup_stub.c" -o "$BUILD/startup_stub.o" 2>/dev/null
clang "${CFLAGS[@]}" -c "$BUILD/halstub.c"     -o "$BUILD/halstub.o"     2>/dev/null

if ld.lld -Ttext=0x08000000 --entry=main --unresolved-symbols=ignore-all \
        -o "$BUILD/fw.elf" "$BUILD"/*.o 2>/dev/null; then
    size "$BUILD/fw.elf"
else
    echo "(link step unavailable; per-object sizes above are still valid)"
fi
