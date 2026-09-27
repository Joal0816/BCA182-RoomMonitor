#!/usr/bin/env python3
"""Sanity-check platformio.ini against the project's own lib/ directory.

PlatformIO resolves `lib_deps` entries through the *registry*.  A library that
lives in the project's own `lib/` directory is discovered automatically and must
NOT also be named in `lib_deps`: doing so makes PlatformIO try to fetch it from
the registry, fail, and abort the build before the link step.  The symptom is
nasty because it is silent -- every object compiles, then the build simply stops
with no `firmware.elf`, and Wokwi (which loads the last built binary) shows a
dead board.

This check catches that mistake, plus a few neighbouring ones, without needing
PlatformIO installed.

Exit status is 0 when the configuration is consistent, 1 otherwise.
"""

import configparser
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INI = os.path.join(ROOT, "platformio.ini")
LIBDIR = os.path.join(ROOT, "lib")

problems = []
notes = []


def local_library_names():
    """Names of libraries present under lib/, from library.json or the dir name."""
    names = set()
    if not os.path.isdir(LIBDIR):
        return names
    for entry in sorted(os.listdir(LIBDIR)):
        path = os.path.join(LIBDIR, entry)
        if not os.path.isdir(path):
            continue
        manifest = os.path.join(path, "library.json")
        if os.path.isfile(manifest):
            try:
                with open(manifest, encoding="utf-8") as fh:
                    names.add(json.load(fh)["name"])
                continue
            except Exception as exc:  # noqa: BLE001
                problems.append("lib/%s/library.json is not valid JSON: %s" % (entry, exc))
        names.add(entry)
    return names


def parse_ini():
    """Return {section: {key: raw_value}} preserving multi-line values."""
    if not os.path.isfile(INI):
        problems.append("platformio.ini not found at %s" % INI)
        return {}
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    try:
        parser.read(INI, encoding="utf-8")
    except Exception as exc:  # noqa: BLE001
        problems.append("platformio.ini could not be parsed: %s" % exc)
        return {}
    return {s: dict(parser.items(s)) for s in parser.sections()}


def main():
    local = local_library_names()
    sections = parse_ini()

    if not sections:
        for p in problems:
            print("  FAIL  %s" % p)
        return 1

    # --- the bug this script exists for -------------------------------------
    for section, opts in sections.items():
        raw = opts.get("lib_deps", "")
        entries = [ln.strip() for ln in raw.splitlines() if ln.strip()]
        for entry in entries:
            # A registry spec is "owner/Name@version" or "Name@version" or a URL.
            bare = entry.split("@", 1)[0].strip()
            if "/" in bare or bare.startswith(("http", "git", "file:")):
                continue
            if bare in local:
                problems.append(
                    "%s: lib_deps names '%s', which is a local library in lib/. "
                    "PlatformIO will try to fetch it from the registry and abort "
                    "the build before linking. Remove it from lib_deps; local "
                    "libraries are discovered automatically." % (section, bare)
                )

    # --- neighbouring mistakes ----------------------------------------------
    device = sections.get("env:bluepill_f103c8", {})
    native = sections.get("env:native", {})

    if device:
        flags = device.get("build_flags", "")
        if "-DSTM32F103xB" not in flags:
            problems.append("env:bluepill_f103c8: build_flags is missing -DSTM32F103xB")
        if "-DUSE_HAL_DRIVER" not in flags:
            problems.append("env:bluepill_f103c8: build_flags is missing -DUSE_HAL_DRIVER")
        if "-Wl,-u,_printf_float" not in flags:
            notes.append(
                "env:bluepill_f103c8: -Wl,-u,_printf_float is absent, so '%.1f' "
                "will print the literal conversion text instead of a number"
            )
        if "extra_scripts" not in device:
            notes.append(
                "env:bluepill_f103c8: no extra_scripts, so the port-patch guard "
                "will not run and a missing Wokwi workaround stays silent"
            )

    # The patched port is Cortex-M3 assembly; compiling it for the host fails.
    if native and "freertos_port_patch" not in native.get("lib_ignore", ""):
        problems.append(
            "env:native: lib_ignore does not exclude freertos_port_patch, so the "
            "Cortex-M3 port would be compiled for the host"
        )

    # --- report --------------------------------------------------------------
    for n in notes:
        print("  NOTE  %s" % n)
    for p in problems:
        print("  FAIL  %s" % p)

    if problems:
        print("\nconfig check: %d problem(s)" % len(problems))
        return 1

    print("config check: platformio.ini is consistent with lib/ (%d local libraries)"
          % len(local))
    return 0


if __name__ == "__main__":
    sys.exit(main())
