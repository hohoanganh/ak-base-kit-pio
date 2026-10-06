#!/usr/bin/env python3
"""
new_project.py - export a standalone firmware project from ak-mcu-base.

ak-mcu-base borrows SPL/CMSIS and the board file from the parent repo. A
product project must not depend on that tree, so this script copies the base
plus the vendor files it needs into one self-contained folder:

  python tools/new_project.py D:/work/my-product --board my-board

  my-product/
    kernel/ hal/ common/ services/ boot/ app/ port/ tests/ tools/ docs/ ...
    vendor/stm32l1/   SPL + CMSIS headers (only what the build uses)
    boards/           PlatformIO board file
    BASE_VERSION      which ak-mcu-base the project started from

Nothing is written into the base. The destination must not exist or be empty.
After the export:  cd <dest>  ->  pio run   (or the CMake targets in Makefile).
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time

BASE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPO = os.path.normpath(os.path.join(BASE, ".."))
LIBS = os.path.join(REPO, "sources", "application", "platform", "stm32l", "Libraries")

SKIP_DIRS = {"build", ".pio", "__pycache__", "vendor", "boards"}
SKIP_FILES = {"ak_sim_flash.bin"}
SKIP_EXT = {".img", ".pyc"}

# vendor files the build needs: (source below Libraries/, destination below vendor/stm32l1/)
VENDOR = [
    (os.path.join("STM32L1xx_StdPeriph_Driver", "inc"), os.path.join("STM32L1xx_StdPeriph_Driver", "inc")),
    (os.path.join("STM32L1xx_StdPeriph_Driver", "src"), os.path.join("STM32L1xx_StdPeriph_Driver", "src")),
    (os.path.join("CMSIS", "Include"), os.path.join("CMSIS", "Include")),
    (os.path.join("CMSIS", "Device", "ST", "STM32L1xx", "Include"),
     os.path.join("CMSIS", "Device", "ST", "STM32L1xx", "Include")),
]


def fail(msg):
    print("error: " + msg, file=sys.stderr)
    sys.exit(1)


def base_version():
    """Tag/commit of the base, for BASE_VERSION. Works without git too."""
    try:
        d = subprocess.run(["git", "-C", BASE, "describe", "--tags", "--always", "--dirty",
                            "--match", "ak-mcu-base-v*"],
                           capture_output=True, text=True, timeout=10)
        if d.returncode == 0 and d.stdout.strip():
            return d.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return "unknown (no git)"


def copy_base(dest):
    n = 0
    for root, dirs, files in os.walk(BASE):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        rel = os.path.relpath(root, BASE)
        out = dest if rel == "." else os.path.join(dest, rel)
        os.makedirs(out, exist_ok=True)
        for f in files:
            if f in SKIP_FILES or os.path.splitext(f)[1] in SKIP_EXT:
                continue
            shutil.copy2(os.path.join(root, f), os.path.join(out, f))
            n += 1
    return n


def edit(path, subs):
    """Apply (pattern, replacement) pairs; every pattern must match exactly once."""
    with open(path, encoding="utf-8", newline="") as f:
        s = f.read()
    for pat, rep in subs:
        s, k = re.subn(pat, rep, s, count=1, flags=re.M)
        if k != 1:
            fail("%s: pattern not found: %s" % (os.path.basename(path), pat))
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(s)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dest", help="new project folder (must not exist or be empty)")
    ap.add_argument("--board", help="board name stored in the image header, max 15 chars "
                                    "(default: keep the base's name). Images built for another "
                                    "board name are refused by the bootloader.")
    a = ap.parse_args(argv)

    dest = os.path.abspath(a.dest)
    if os.path.commonpath([dest, REPO]) == REPO:
        fail("destination is inside the base repository")
    if os.path.exists(dest) and os.listdir(dest):
        fail("destination exists and is not empty: " + dest)
    if a.board and not re.fullmatch(r"[A-Za-z0-9_.-]{1,15}", a.board):
        fail("--board: 1..15 characters of A-Z a-z 0-9 _ . -")
    if not os.path.isdir(os.path.join(LIBS, "STM32L1xx_StdPeriph_Driver")):
        fail("SPL not found below " + LIBS)

    n = copy_base(dest)
    for src, dst in VENDOR:
        shutil.copytree(os.path.join(LIBS, src), os.path.join(dest, "vendor", "stm32l1", dst))
    os.makedirs(os.path.join(dest, "boards"))
    shutil.copy2(os.path.join(REPO, "boards", "genericSTM32L151CB_bare.json"), os.path.join(dest, "boards"))

    # the only path settings of the build: PlatformIO reads them from platformio.ini,
    # CMake picks vendor/stm32l1 by itself when the folder exists (port.cmake)
    edit(os.path.join(dest, "platformio.ini"), [
        (r"^boards_dir = .*$", "boards_dir = boards"),
        (r"^spl = .*$", "spl = vendor/stm32l1"),
    ])
    if a.board:
        edit(os.path.join(dest, "port", "stm32l151", "port_cfg.h"), [
            (r'^(#define PORT_BOARD_NAME\s+)"[^"]*"', r'\1"%s"' % a.board),
        ])

    ver = base_version()
    with open(os.path.join(dest, "BASE_VERSION"), "w", encoding="utf-8", newline="\n") as f:
        f.write("base: ak-mcu-base %s\n" % ver)
        f.write("source: https://github.com/hohoanganh/ak-base-kit-pio (folder ak-mcu-base)\n")
        f.write("exported: %s\n" % time.strftime("%Y-%m-%d"))
        if a.board:
            f.write("board name: %s\n" % a.board)

    print("exported %d base files + vendor SPL/CMSIS to %s" % (n, dest))
    print("base version: " + ver)
    print("next: cd \"%s\" && pio run        (bootloader: pio run -e boot)" % dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
