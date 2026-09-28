#!/usr/bin/env bash
#
# Run the real DHT22 driver (src/app/hal/dht22.c) against a host-side model of
# the sensor, with no STM32 and no wall-clock timing.
#
# dht22.c polls GPIO_TypeDef.IDR directly and times every pulse with
# DWT->CYCCNT, so the model works by force-including a prefix that turns the DWT
# the stub header declares into a macro advancing a virtual microsecond clock
# ("-include dht22_sim_prefix.h").  The data line is then a pure function of that
# clock, which makes each frame deterministic.
#
# The pass is count-locked to the number of RUN_TEST() lines in
# test_dht22_sim.c: a new test that silently does not run changes the count and
# fails the pass.
#
# Usage: tools/verify/run_dht22_sim.sh        (from the repository root)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SIM="$ROOT/tools/verify/dht22_sim"
STUB="$ROOT/tools/verify/stub"
SHIM="$ROOT/tools/verify/shim"
SRC="$ROOT/src"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

EXPECTED=16
INC=(-I"$SIM" -I"$STUB" -I"$SHIM" -I"$SRC" -I"$SRC/app" -I"$SRC/app/hal")

# dht22.c gets the DWT/CoreDebug redirection; every other unit compiles plainly.
if ! gcc -std=c99 -w -nostdinc "${INC[@]}" \
        -include "$SIM/dht22_sim_prefix.h" \
        -c "$SRC/app/hal/dht22.c" -o "$BUILD/dht22.o" 2>"$BUILD/dht22.log"; then
    echo "FAIL: dht22.c did not compile against the simulation"
    sed 's/^/      /' "$BUILD/dht22.log"
    exit 1
fi

for f in "$SIM/sim_hal.c" "$SIM/test_dht22_sim.c" "$SHIM/shim.c"; do
    obj="$BUILD/$(basename "${f%.c}").o"
    if ! gcc -std=c99 -w -nostdinc "${INC[@]}" -c "$f" -o "$obj" 2>"$obj.log"; then
        echo "FAIL: $(basename "$f") did not compile"
        sed 's/^/      /' "$obj.log"
        exit 1
    fi
done

if ! gcc "$BUILD"/*.o -o "$BUILD/dht22_sim" 2>"$BUILD/link.log"; then
    echo "FAIL: the simulation did not link"
    sed 's/^/      /' "$BUILD/link.log"
    exit 1
fi

out="$("$BUILD/dht22_sim")"
rc=$?
got="$(printf '%s\n' "$out" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+')"

if [ "$rc" -ne 0 ] || [ "$got" != "$EXPECTED" ]; then
    echo "FAIL: dht22 host simulation expected $EXPECTED passing, got ${got:-0}"
    printf '%s\n' "$out" | sed 's/^/      /'
    exit 1
fi

printf 'ok   dht22 host simulation   %s passed\n' "$got"
