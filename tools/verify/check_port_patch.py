"""Post-build guard for the project-local FreeRTOS Cortex-M port.

This project ships its own ARM_CM3 port in two halves:

  * `lib/freertos_port_patch/src/port.c` -- shadows the FreeRTOS archive's
    `port.o` and implements the scheduler primitives without PendSV;
  * `include/portmacro.h` -- shadows the archive's header so the kernel core
    (`tasks.c`, `queue.c`, `timers.c`, `event_groups.c`) compiles against those
    primitives instead of the stock BASEPRI/PendSV macros.

Both halves are required.  Each alone is silent-death: with only the header the
linker pulls in the unpatched `port.o`, and with only `port.c` the kernel still
emits PendSV yields.  The failure mode this guards against is therefore a
silently dead firmware -- if the library is ever dropped from the build (a
changed `lib_ldf_mode`, a renamed directory, a `lib_ignore` that leaks into the
device env) the build still succeeds and the board just never runs a task.

Every step is wrapped so that a problem in the check itself can never fail the
build -- it only ever prints.

This file is a PlatformIO `extra_scripts` hook, not a standalone tool: it needs
the SCons `env` object that PlatformIO injects.  Running it directly with
`python3` therefore cannot work, and it says so rather than raising NameError.
"""

try:
    Import("env")
except NameError:
    import sys

    sys.stderr.write(
        "[port-patch] this is a PlatformIO post-build hook, not a standalone script.\n"
        "[port-patch] it runs automatically during `pio run`; see tools/verify/README.md.\n"
    )
    sys.exit(2)

import glob
import os
import re
import subprocess

# Macros the port's header must redirect for the kernel core to compile against
# the patched port.  If any of these is missing, `include/portmacro.h` is not
# shadowing the archive header and the build is using the stock port.
REQUIRED_MACROS = (
    "portYIELD",
    "portEND_SWITCHING_ISR",
    "portYIELD_FROM_ISR",
    "portDISABLE_INTERRUPTS",
    "portENABLE_INTERRUPTS",
    "portENTER_CRITICAL",
    "portEXIT_CRITICAL",
)

# ICSR base address.  A raw store of 0x10000000 (PENDSVSET) to 0xE000ED04 in the
# image means some translation unit compiled against the *stock* `portmacro.h`
# and is raising a yield through PendSV -- which this port does not service, so
# it vectors to the weak `Default_Handler` and wedges the MCU.  The fix is to
# call `vPortYieldFromISR()` directly from an `extern` declaration, which is the
# only way to bypass the stock header.
PENDSVSET_IMMEDIATE = "e000ed04"

# The application calls FreeRTOS object-creation APIs (`xTaskCreate`,
# `xSemaphoreCreateMutex`) before `vTaskStartScheduler()`.  Those reach
# `vPortEnterCritical()` while `uxCriticalNesting` still holds its 0xaaaaaaaa
# "kernel not started" sentinel, so the counter can never return to 0 and the
# SysTick gate applied on entry would never be lifted -- freezing the HAL tick
# and hanging every subsequent `HAL_Delay()`.  Both halves of the critical
# section must therefore defer to the scheduler state.
PRESCHEDULER_GUARD = "xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED"

def _check_header(project_dir):
    header = os.path.join(project_dir, "include", "portmacro.h")
    if not os.path.isfile(header):
        return ["include/portmacro.h is MISSING"]

    with open(header, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    missing = [name for name in REQUIRED_MACROS if ("#define " + name) not in text]
    if missing:
        return ["include/portmacro.h does not define: " + ", ".join(missing)]
    return []

def _function_body(text, name):
    """Return the body of a C function `name`, or None if it is not found."""
    match = re.search(
        r"\b" + re.escape(name) + r"\s*\(\s*(?:void\s*)?\)\s*\{", text
    )
    if not match:
        return None
    depth = 0
    for index in range(match.end() - 1, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[match.end():index]
    return None

def _check_critical_sections(port_source):
    """Both critical-section primitives must bail out before the scheduler."""
    if not os.path.isfile(port_source):
        return ["lib/freertos_port_patch/src/port.c is MISSING"]

    with open(port_source, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    problems = []
    for name in ("vPortEnterCritical", "vPortExitCritical"):
        body = _function_body(text, name)
        if body is None:
            problems.append("%s() not found in port.c" % name)
        elif PRESCHEDULER_GUARD not in body:
            problems.append(
                "%s() is missing the pre-scheduler guard; a critical section "
                "taken before vTaskStartScheduler() would gate SysTick forever"
                % name
            )
    return problems

def _objdump(env, elf):
    tool = env.subst("$OBJDUMP") if env else ""
    if not tool or not os.path.isfile(tool):
        candidates = sorted(
            glob.glob(
                os.path.expanduser(
                    "~/.platformio/packages/toolchain-*/bin/arm-none-eabi-objdump"
                )
            )
        )
        tool = candidates[0] if candidates else None
    if not tool:
        return None
    try:
        out = subprocess.run(
            [tool, "-d", elf], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
        )
        return out.stdout.decode("utf-8", "replace")
    except Exception:  # noqa: BLE001 - a broken probe must not break the build
        return None

def _check_no_pendsv_store(env, elf):
    disassembly = _objdump(env, elf)
    if disassembly is None:
        return []
    hits = disassembly.lower().count(PENDSVSET_IMMEDIATE)
    if hits:
        return [
            "firmware still writes ICSR (0xE000ED04) %d time(s): a translation "
            "unit compiled against the stock portmacro.h and will raise PendSV, "
            "which this port does not service" % hits
        ]
    return []


def _check_port_patch(source, target, env):
    try:
        build_dir = env.subst("$BUILD_DIR")
        project_dir = env.subst("$PROJECT_DIR")

        problems = _check_header(project_dir)
        problems += _check_critical_sections(
            os.path.join(project_dir, "lib", "freertos_port_patch", "src", "port.c")
        )

        hits = glob.glob(
            os.path.join(build_dir, "**", "freertos_port_patch", "port.o"),
            recursive=True,
        )
        if not hits:
            problems.append("lib/freertos_port_patch/src/port.c was NOT compiled")

        elf = target[0].get_abspath() if target else os.path.join(
            build_dir, env.subst("${PROGNAME}.elf")
        )
        if os.path.isfile(elf):
            problems += _check_no_pendsv_store(env, elf)
        else:
            problems.append("firmware .elf not found, cannot disassemble-check PendSV")

        if problems:
            print("[port-patch] WARNING: the patched Cortex-M port is incomplete.")
            for problem in problems:
                print("[port-patch] WARNING: " + problem)
            print("[port-patch] WARNING: the stock port will be used instead; expect")
            print("[port-patch] WARNING: the firmware to print its banner and then hang.")
        else:
            print(
                "[port-patch] OK: patched ARM_CM3 port.o + include/portmacro.h in the "
                "image, no ICSR PendSV store, pre-scheduler critical sections deferred"
            )
    except Exception as exc:  # noqa: BLE001 - a broken check must not break the build
        print("[port-patch] check skipped: %s" % exc)


try:
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", _check_port_patch)
except Exception as exc:  # noqa: BLE001
    print("[port-patch] could not register post-build check: %s" % exc)
