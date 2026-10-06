#!/usr/bin/env python3
"""
End-to-end test on ak_sim (simulated bootloader + app) using the real
tools/ak_fw.py and tools/mkimage.py, as when flashing a board:

  1. blank flash -> bootloader in loader mode
  2. upload v1.0.0 via loader -> boot installs -> app v1.0.0 runs
  3. OTA v2.1.0 via app -> reset -> boot installs -> app v2.1.0
  4. wrong-board image rejected, app keeps running
  5. shell 'ver' reports the right version
  6. restart the process: still v2.1.0 (flash persisted)
  7. LOADER from app -> bootloader; RUN -> back to app
"""
import argparse
import os
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", required=True)
    ap.add_argument("--tools", required=True)
    ap.add_argument("--workdir", required=True)
    a = ap.parse_args()

    sys.path.insert(0, a.tools)
    import ak_fw
    import mkimage

    flash = os.path.join(a.workdir, "sim_ota_flash.bin")
    img1 = os.path.join(a.workdir, "sim_v1.img")
    img2 = os.path.join(a.workdir, "sim_v2.img")
    img_bad = os.path.join(a.workdir, "sim_bad.img")
    mkimage.main(["synthetic", "-o", img1, "--version", "1.0.0", "--size", "9000", "--seed", "1"])
    mkimage.main(["synthetic", "-o", img2, "--version", "2.1.0", "--size", "20000", "--seed", "2"])
    mkimage.main(["synthetic", "-o", img_bad, "--version", "9.9.9", "--board", "other", "--seed", "3"])

    fails = []

    def check(cond, what):
        print(("  ok   " if cond else "  FAIL ") + what)
        if not cond:
            fails.append(what)

    def wait_role(fw, role, ver=None, timeout=20.0):
        import time
        end = time.time() + timeout
        while time.time() < end:
            try:
                i = fw.info(timeout=0.3, retries=1)
                if i["role"] == role and (ver is None or i["version"] == ver):
                    return i
            except ak_fw.FwError:
                pass
        return None

    # --- 1..5 ---
    fw = ak_fw.AkFw(ak_fw.SimLink([a.sim, "--flash", flash, "--fresh"]))
    try:
        i = fw.wait_ready(5)
        check(i["role"] == "bootloader", "blank flash -> loader (%s)" % i["role"])
        check(i["board"] == "ak-host", "board name")

        ver, _ = fw.upload(open(img1, "rb").read())
        check(ver == "1.0.0", "upload v1 via loader verified")
        fw.install()
        check(wait_role(fw, "app", "1.0.0") is not None, "boot installed v1, app running")

        ver, size = fw.upload(open(img2, "rb").read())
        check(ver == "2.1.0" and size == 20000, "OTA upload v2 via app verified")
        fw.install()
        check(wait_role(fw, "app", "2.1.0") is not None, "boot installed v2, app running")

        try:
            fw.upload(open(img_bad, "rb").read())
            check(False, "wrong board image rejected")
        except ak_fw.FwError as e:
            check("wrong board" in str(e), "wrong board image rejected (%s)" % e)
        check(wait_role(fw, "app", "2.1.0") is not None, "app still v2 after rejected image")

        fw.clear_text()
        fw.send_text("ver\r")
        check(fw.wait_text("app v2.1.0"), "shell 'ver' -> v2.1.0")
        fw.send_text("info\r")
        check(fw.wait_text("installs 2"), "shell 'info' -> 2 installs")
    finally:
        fw.close()

    # --- 6..7: restart, flash persisted ---
    fw = ak_fw.AkFw(ak_fw.SimLink([a.sim, "--flash", flash]))
    try:
        check(wait_role(fw, "app", "2.1.0") is not None, "restart -> straight into app v2")
        fw.simple(ak_fw.CMD_LOADER, "LOADER")
        check(wait_role(fw, "bootloader") is not None, "LOADER -> bootloader")
        fw.simple(ak_fw.CMD_RUN, "RUN")
        check(wait_role(fw, "app", "2.1.0") is not None, "RUN -> back to app v2")
    finally:
        fw.close()

    print("%d failed" % len(fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
