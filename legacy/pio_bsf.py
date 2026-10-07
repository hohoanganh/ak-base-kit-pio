# pio_bsf.py
# Sinh + nap "boot share flash" (BSF) o 0x08002000.
#
# Bootloader (sources/boot/app/app.cpp) chi nhay sang app khi doc duoc trong
# BSF:  fw_app_cmd.cmd == SYS_BOOT_CMD_NONE  VA
#       current_fw_app_header.psk == FIRMWARE_PSK
# Nguoc lai no roi vao nhanh "unexpected status" -> while(1) nhap nhay LED,
# nhin tu ngoai giong het board bi treo.
#
# TU BOOTLOADER 0.0.2 (base v1.1.0) KHONG CON BAT BUOC: bootloader tu kiem
# bang vector cua app va tu va BSF (docs/known-bugs.md #3). Target nay giu
# lai cho board con mang bootloader cu.
#
# BSF chi duoc ghi boi luong update qua UART bootloader / OTA. Khi nap thang
# app bang SWD (pio run -e app -t upload) thi BSF van trang (0xFF) hoac 0x00
# -> boot khong bao gio nhay sang app. Target nay ghi mot ban BSF hop le de
# board boot thang vao app.
#
#   pio run -e app -t bsf
#
# Layout sys_boot_t (sources/boot/sys/sys_boot.h, ARM EABI, khong packed):
#   0  current_fw_boot_header  {u32 psk, u32 bin_len, u16 checksum + pad} 12B
#   12 current_fw_app_header   12B
#   24 update_fw_boot_header   12B
#   36 update_fw_app_header    12B
#   48 fw_boot_cmd  {u8 cmd, u8 container, u8 io_driver, pad, u32 des_addr,
#                    u32 src_addr, ak_msg_host_res_t(6B, packed), pad}  20B
#   68 fw_app_cmd              20B
#   -> sizeof = 88
Import("env")

import os
import struct
import subprocess

BSF_ADDR = 0x08002000
SYS_BOOT_SIZE = 88
OFF_CURRENT_FW_APP_HEADER = 12
OFF_FW_BOOT_CMD = 48
OFF_FW_APP_CMD = 68
FIRMWARE_PSK = 0x1A2B3C4D
SYS_BOOT_CMD_NONE = 0x01


def _seed_path():
    dest_dir = os.path.join(env.subst("$PROJECT_DIR"), "release", "bsf")
    os.makedirs(dest_dir, exist_ok=True)
    return os.path.join(dest_dir, "bsf_seed.bin")


def _write_seed():
    blob = bytearray(SYS_BOOT_SIZE)
    struct.pack_into("<I", blob, OFF_CURRENT_FW_APP_HEADER, FIRMWARE_PSK)
    blob[OFF_FW_BOOT_CMD] = SYS_BOOT_CMD_NONE
    blob[OFF_FW_APP_CMD] = SYS_BOOT_CMD_NONE

    path = _seed_path()
    with open(path, "wb") as f:
        f.write(bytes(blob))
    return path


def upload_bsf(target, source, env):
    path = _write_seed()
    print("[pio_bsf] BSF seed: %s" % path)

    # Tai su dung tham so server openocd ma platform da dung cho -t upload;
    # 2 phan tu cuoi la ("-c", "program {...}") -> thay bang lenh cua rieng BSF.
    flags = list(env["UPLOADERFLAGS"])[:-2]
    flags += ["-c", "program {%s} 0x%08X verify reset; shutdown;"
              % (path.replace("\\", "/"), BSF_ADDR)]

    cmd = [env.subst("$UPLOADER")] + flags
    proc_env = dict(os.environ)
    proc_env.update({k: v for k, v in env["ENV"].items() if isinstance(v, str)})
    return subprocess.call(cmd, env=proc_env)


if env["PIOENV"] == "app":
    env.AddCustomTarget(
        name="bsf",
        dependencies=None,
        actions=[upload_bsf],
        title="Seed boot share flash",
        description="Ghi BSF hop le o 0x08002000 de bootloader nhay sang app",
    )
