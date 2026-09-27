"""Post-build guard for the Wokwi FreeRTOS port workaround.

`lib/freertos_port_patch` shadows the FreeRTOS archive's own ARM_CM3 `port.o`
with a project-local copy that clears PRIMASK/FAULTMASK before the `svc 0` that
starts the first task.  Wokwi's Cortex-M3 model sets those mask bits on `cpsie`
instead of clearing them, so without the workaround the `svc` is masked and no
task ever runs.

The failure mode this guards against is silent: if the library is ever dropped
from the build (a changed `lib_ldf_mode`, a renamed directory, a `lib_ignore`
that leaks into the device env), the linker quietly falls back to the unpatched
archive member and the firmware goes back to looking dead on boot with no
diagnostic.  This script makes that visible in the build log.

Every step is wrapped so that a problem in the check itself can never fail the
build -- it only ever prints.
"""

Import("env")

import glob
import os


def _check_port_patch(source, target, env):
    try:
        build_dir = env.subst("$BUILD_DIR")
        hits = glob.glob(
            os.path.join(build_dir, "**", "freertos_port_patch", "port.o"),
            recursive=True,
        )
        if hits:
            print("[port-patch] OK: patched ARM_CM3 port.o is in the image")
        else:
            print("[port-patch] WARNING: lib/freertos_port_patch/src/port.c was NOT compiled.")
            print("[port-patch] WARNING: the Wokwi PRIMASK/FAULTMASK workaround is missing,")
            print("[port-patch] WARNING: so the first task will not start in the simulator.")
    except Exception as exc:  # noqa: BLE001 - a broken check must not break the build
        print("[port-patch] check skipped: %s" % exc)


try:
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", _check_port_patch)
except Exception as exc:  # noqa: BLE001
    print("[port-patch] could not register post-build check: %s" % exc)
