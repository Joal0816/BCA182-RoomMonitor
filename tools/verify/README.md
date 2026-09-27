# Verification harness

This directory contains everything needed to re-derive, from the source in `src/`,
the three reproducible results quoted in the laboratory report:

| Claim | Where quoted | Reproducible? |
|---|---|---|
| 33 native unit tests pass | report §5.1, README | **Yes** — `run_tests.sh` |
| Static analysis: 2 benign MMIO findings, 21 advisories, 1 warning | report §6, `docs/static-analysis.md` | **Yes** — `run_static_analysis.sh` |
| Firmware size / RAM use | report §7.1 | **Approximately** — `run_size_analysis.sh` |
| `platformio.ini` is consistent with `lib/` | — | **Yes** — `check_config.py` |

All four scripts are self-contained: they need only `gcc`, `clang`, `clang-tidy`
and Python 3, and they never touch the network or the real STM32Cube/FreeRTOS
packages.

One further script is **not** part of `run_all.sh`, because it cannot run in an
environment without PlatformIO:

| Script | Purpose | Requires |
|---|---|---|
| `check_port_patch.py` | Post-build guard. Registered in `platformio.ini` via `extra_scripts`; globs `**/freertos_port_patch/port.o` and prints `[port-patch] OK`, so a build that silently fell back to the stock FreeRTOS port is caught. It is a PlatformIO hook, not a standalone tool — running it with `python3` directly prints a notice and exits 2, because it needs the SCons `env` object PlatformIO injects. | `pio run` |
| `../generate_wiring_diagram.py` | Renders `docs/wiring-diagram.png` directly from `diagram.json`, so the schematic cannot drift from the wiring. | Python 3 + Pillow |

## Why `check_config.py` exists

PlatformIO resolves `lib_deps` entries through the **registry**. A library that
lives in the project's own `lib/` directory is discovered automatically and must
**not** also be named in `lib_deps`: doing so makes PlatformIO try to fetch it
from the registry, fail, and abort the build *before the link step*.

The failure mode is unusually nasty because it is silent. Every object file
compiles, the CMSIS and FreeRTOS archives are created, and then the build simply
stops — no `firmware.elf`, no `firmware.bin`, and no error that names the cause.
Wokwi loads the last successfully built binary, so the visible symptom is a dead
board rather than a build error.

`check_config.py` catches that mistake, plus a few neighbouring ones (missing
`-DSTM32F103xB`, a `[env:native]` that would try to compile the Cortex-M3 port
for the host), without needing PlatformIO installed. It is a static check of the
configuration against the contents of `lib/`.

## Why stub headers exist

`stub/` contains hand-written replacement headers for `stm32f1xx_hal.h` and the
FreeRTOS API. They exist because the environment used for verification has
neither the STM32Cube HAL sources nor a system C library (`/usr/include` is
empty). They declare the HAL and FreeRTOS symbols with signatures derived from
the call sites in `src/`, which is enough for the compiler to type-check the
application code.

**The stub is an approximation of the interface, not a verification of it.** Any
defect that depends on the real macro expansion or the real API contract is
invisible to these passes. Same for `shim/`, which supplies `stdint.h`,
`string.h` and a small Unity-compatible test runner so the `test/` sources
compile without a system libc.

## Usage

```bash
./tools/verify/run_all.sh              # every pass below, in one command

./tools/verify/run_tests.sh            # 33 native unit tests
./tools/verify/run_static_analysis.sh  # clang --analyze, clang-tidy, -Wall -Wextra
./tools/verify/run_size_analysis.sh    # ARM object sizes + linked ELF estimate
python3 tools/verify/check_config.py   # platformio.ini vs. the contents of lib/

python3 tools/generate_wiring_diagram.py   # regenerate docs/wiring-diagram.png
```

Each exits non-zero on failure. Run them from the repository root.

`run_static_analysis.sh` is **count-locked**: it asserts exactly 2 analyzer
findings, 21 clang-tidy advisories and 1 compiler warning. The counts are
deliberate — the 2 analyzer findings are the two intentional memory-mapped
register accesses, and the 1 warning is a pre-existing signed/unsigned
comparison. A change in any count fails the pass, which is the point: it makes
an unnoticed new finding impossible to merge.

## What is *not* covered here

The Wokwi simulation runs (`docs/functional-verification.md`) and the fault
experiments (`docs/fault-experiments.md`) cannot be replayed by these scripts.
See the "Evidence limits" section of `docs/limitations.md`.
