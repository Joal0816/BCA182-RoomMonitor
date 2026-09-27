#!/usr/bin/env bash
#
# Run every verification pass for the laboratory report in one command.
#
#   1. Firmware build + real memory footprint  (requires PlatformIO; skipped if absent)
#   2. Native unit tests                       (33 expected)
#   3. Static analysis                         (1 + 21 + 1 findings expected)
#   4. Application-only size breakdown         (clang, indicative)
#   5. platformio.ini consistency              (no local library named in lib_deps)
#
# Exits non-zero if any executed pass fails. Passes that cannot run on the
# current host are reported as SKIPPED and do not fail the run.
#
# Usage: tools/verify/run_all.sh        (from the repository root)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PASSED=()
FAILED=()
SKIPPED=()

banner() { printf '\n\033[1m=== %s ===\033[0m\n' "$1"; }

# --- 1. Real build (needs PlatformIO + the ARM toolchain) --------------------
banner "1/5  Firmware build and memory footprint (PlatformIO)"
if command -v pio >/dev/null 2>&1; then
    if pio run -e bluepill_f103c8 2>&1 | tail -4; then
        PASSED+=("build")
        echo
        echo "Section breakdown (size -A), authoritative for the flashed artifact:"
        size -A .pio/build/bluepill_f103c8/firmware.elf 2>/dev/null \
            | awk '/^\.(text|rodata|data|bss|isr_vector)/{printf "    %-14s %8s\n", $1, $2}'
        if [ -f .pio/build/bluepill_f103c8/firmware.bin ]; then
            binsz=$(stat -c '%s' .pio/build/bluepill_f103c8/firmware.bin)
            printf '    %-14s %8s  <- true flashed image\n' 'firmware.bin' "$binsz"
        fi
    else
        FAILED+=("build")
    fi
else
    SKIPPED+=("build (pio not installed)")
    echo "pio not found on PATH -- skipping the firmware build."
    echo "Install PlatformIO Core to reproduce the report's section 7.1 figures."
fi

# --- 2. Unit tests ----------------------------------------------------------
banner "2/5  Native unit tests"
if bash tools/verify/run_tests.sh; then PASSED+=("tests"); else FAILED+=("tests"); fi

# --- 3. Static analysis -----------------------------------------------------
banner "3/5  Static analysis"
if bash tools/verify/run_static_analysis.sh; then PASSED+=("static-analysis"); else FAILED+=("static-analysis"); fi

# --- 4. Application-only sizes ---------------------------------------------
banner "4/5  Application-only size breakdown (clang, indicative)"
if bash tools/verify/run_size_analysis.sh; then PASSED+=("sizes"); else FAILED+=("sizes"); fi

# --- 5. Configuration consistency ------------------------------------------
banner "5/5  platformio.ini consistency"
if python3 tools/verify/check_config.py; then PASSED+=("config"); else FAILED+=("config"); fi

# --- Summary ----------------------------------------------------------------
echo
echo "============================================================"
printf 'Verification summary\n'
printf '  passed : %s\n' "${PASSED[*]:-none}"
printf '  failed : %s\n' "${FAILED[*]:-none}"
printf '  skipped: %s\n' "${SKIPPED[*]:-none}"
echo "============================================================"

if [ ${#FAILED[@]} -gt 0 ]; then
    echo "RESULT: FAILED"
    exit 1
fi

echo "RESULT: PASSED"
