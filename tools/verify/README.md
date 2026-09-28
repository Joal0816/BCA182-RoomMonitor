# Verification harness

This directory contains everything needed to re-derive, from the source in `src/`,
the three reproducible results quoted in the laboratory report:

| Claim | Where quoted | Reproducible? |
|---|---|---|
| 33 native unit tests pass | report §5.1, README | **Yes** — `run_tests.sh` |
| DHT22 frame decode, checksum rejection and timeout behaviour | — | **Yes** — `dht22_sim/run_dht22_sim.sh` |
| Static analysis: 2 benign MMIO findings, 21 advisories, 1 warning | report §6, `docs/static-analysis.md` | **Yes** — `run_static_analysis.sh` |
| Firmware size / RAM use | report §7.1 | **Approximately** — `run_size_analysis.sh` |
| `platformio.ini` is consistent with `lib/` | — | **Yes** — `check_config.py` |
| `diagram.json` is a valid Wokwi circuit | — | **Yes** — `wokwi-cli lint` (skipped if `wokwi-cli` is absent) |
| `docs/laboratory-report.pdf` matches the Markdown | — | **Yes** — `generate_report_pdf.py --check` (skipped if WeasyPrint is absent) |

The first four scripts are self-contained: they need only `gcc`, `clang`,
`clang-tidy` and Python 3, and they never touch the network or the real
STM32Cube/FreeRTOS packages.

Two passes have external dependencies. The Wokwi diagram lint needs `wokwi-cli`
(`https://github.com/wokwi/wokwi-cli/releases`), and the PDF freshness check
needs `weasyprint` and `markdown` (`pip install weasyprint markdown`) plus
`pdftotext`. `run_all.sh` reports either pass as *skipped* rather than failed
when the dependency is not present, so the harness still passes on a machine
that has neither. Note that `lint --offline` needs no CI token; only
`wokwi-cli test`, which actually executes the firmware, does.

The PDF check re-renders the report Markdown into a temporary file and compares
the extracted text against the committed PDF, so a Markdown edit that was not
followed by `python3 tools/generate_report_pdf.py` fails loudly. The comparison
is on text rather than bytes because PDF streams are not reproducible run to
run. Always commit the Markdown and the PDF together.

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

`check_config.py` catches that mistake, plus a few neighbouring ones, without
needing PlatformIO installed. It is a static check of the configuration against
the contents of `lib/`. The full set:

| Check | Why it matters |
|-------|----------------|
| No local library named in `lib_deps` | Registry fetch fails and aborts the build before linking |
| `-DSTM32F103xB` and `-DUSE_HAL_DRIVER` present | Wrong part / no HAL driver selection |
| `-Wl,-u,_printf_float` present | Otherwise `%.1f` prints literal conversion text — a *note*, not a failure |
| `extra_scripts` present | Otherwise the port-patch guard never runs and a missing Wokwi workaround stays silent |
| `env:native` ignores `freertos_port_patch` | Cortex-M3 assembly would be compiled for the host |
| `env:native` has `build_src_filter = -<*>` | Otherwise `src/` is compiled for the host and fails on `stm32f1xx_hal.h` |
| `platformio` sets `default_envs = bluepill_f103c8` | Otherwise a bare `pio run` also builds `env:native` and reports FAILED |

The last two guard the same trap from both sides. A bare `pio run` builds every
environment in the file, and `env:native` has no STM32Cube HAL — so without
`default_envs` the firmware itself builds perfectly and the run still reports
`FAILED`, because `stm32f1xx_hal.h` cannot be found. Almost nothing under `src/`
is host-compilable: 16 of the 17 translation units reach the HAL header, the six
driver headers in `src/app/hal/` and `src/main.h` including it directly and the
rest inheriting it through `main.h`, with only `src/drivers/diag.c` free of it.
`pio test -e native` is unaffected: `pio test` does not build `src/` unless
`test_build_src` is enabled.

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

## Why the DHT22 host simulation exists

Unlike the modules under `src/app/logic/`, the DHT22 driver is not
hardware-independent: it polls `GPIO_TypeDef.IDR` directly and measures every
pulse with `DWT->CYCCNT`. `dht22_sim/` compiles the real driver
(`src/app/hal/dht22.c`) unmodified against a host model of the sensor, so the
frame decode, the checksum check and the timeout paths can be exercised without
a board or Wokwi. It is what pins down the driver's own claim that it releases
the line to the internal pull-up rather than leaving it floating.

The injection is contained in `dht22_sim_prefix.h`, which gcc `-include`s into
that one translation unit. It replaces the `DWT` the stub header declares with a
macro that advances a *virtual* microsecond clock on every access, and points
`CoreDebug` at simulator memory so `DHT22_Init()` does not write to
`0xE000EDF0`. The data line is then a pure function of the clock, so the model is
deterministic and never depends on host timing.

**Coverage limit:** the model is faithful to the cycle-counter branch, which is
the branch the firmware takes on real silicon. The no-DWT fallback in
`DHT22_CaptureFrame` — the sample-past-30 us path — is not exercised, because it
makes no forward progress without the counter the model drives.

## Usage

```bash
./tools/verify/run_all.sh              # every pass below, in one command

./tools/verify/run_tests.sh            # 33 native unit tests
./tools/verify/dht22_sim/run_dht22_sim.sh  # 16 DHT22 driver tests against a host sensor model
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

The Wokwi behavioural runs (`docs/functional-verification.md`, the WF table in the
laboratory report, and the fault experiments) cannot be replayed by these scripts. A replay
against the current revision was performed with `wokwi-cli` installed and authenticated and
did **not** reproduce any of them — every session stops after three boot lines with no task
started. See L-07 in `docs/limitations.md` for the outcome and the "Evidence limits" section
of the same file for what the harness does and does not establish.
