#!/usr/bin/env bash
#
# Rebuild and run the native unit tests from test/ without a system libc.
#
# The host has no /usr/include, so the Unity test framework, stdint.h and
# string.h are replaced by tools/verify/shim/. The test sources in test/ are
# used unmodified.
#
# Usage: tools/verify/run_tests.sh        (from the repository root)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SHIM="$ROOT/tools/verify/shim"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

SUITES=(temperature encoder state_machine)
EXPECTED=(15 10 8)
TOTAL=0
FAILED=0

for i in "${!SUITES[@]}"; do
    suite="${SUITES[$i]}"
    want="${EXPECTED[$i]}"
    src="$ROOT/test/test_$suite/test_main.c"

    if [ ! -f "$src" ]; then
        echo "FAIL: missing $src"
        FAILED=1
        continue
    fi

    if ! gcc -std=c99 -w -nostdinc -I"$SHIM" -I"$ROOT/src" \
             "$src" "$SHIM/shim.c" -o "$BUILD/$suite" 2>"$BUILD/$suite.log"; then
        echo "FAIL: test_$suite did not compile"
        sed 's/^/      /' "$BUILD/$suite.log"
        FAILED=1
        continue
    fi

    out="$("$BUILD/$suite")"
    rc=$?
    got="$(printf '%s\n' "$out" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+')"

    if [ "$rc" -ne 0 ] || [ "$got" != "$want" ]; then
        echo "FAIL: test_$suite expected $want passing, got ${got:-0}"
        printf '%s\n' "$out" | sed 's/^/      /'
        FAILED=1
    else
        printf 'ok   test_%-16s %s passed\n' "$suite" "$got"
    fi
    TOTAL=$((TOTAL + ${got:-0}))
done

echo
echo "Total: $TOTAL passing (expected 33)"
exit "$FAILED"
