# pio_copy_release.py
# Sau moi lan build thanh cong, tu dong copy firmware thanh pham (.bin/.elf/
# .hex neu co) tu thu muc build (nam ngoai OneDrive - xem ghi chu build_dir
# trong platformio.ini) ve release/<env>/ trong project. Ten file kem version
# lay tu -DAPP_VERSION de phan biet cac ban build:
#   release/app/ak_base_kit_app_v1.0.0.bin
#   release/boot/ak_base_kit_boot_v1.0.0.bin
Import("env")

import os
import re
import shutil


def _get_version(env):
    """Lay chuoi x.y.z tu define APP_VERSION trong build_flags."""
    for item in env.get("CPPDEFINES", []):
        if isinstance(item, (list, tuple)) and len(item) == 2 and item[0] == "APP_VERSION":
            m = re.search(r"(\d+\.\d+\.\d+)", str(item[1]))
            if m:
                return m.group(1)
    return "unknown"


def copy_release(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("$PROGNAME")          # "firmware"
    env_name = env["PIOENV"]                    # "app" / "boot"
    version = _get_version(env)

    dest_dir = os.path.join(env.subst("$PROJECT_DIR"), "release", env_name)
    os.makedirs(dest_dir, exist_ok=True)

    copied = []
    for ext in (".bin", ".elf", ".hex"):
        src = os.path.join(build_dir, progname + ext)
        if os.path.isfile(src):
            dst = os.path.join(dest_dir,
                               f"ak_base_kit_{env_name}_v{version}{ext}")
            shutil.copy2(src, dst)
            copied.append(os.path.basename(dst))

    if copied:
        print(f"[pio_copy_release] Da copy ve release/{env_name}/: "
              + ", ".join(copied))
    else:
        print("[pio_copy_release] Khong tim thay file thanh pham de copy "
              f"trong {build_dir}")


# Gan post-action vao .bin de chac chan chay sau khi .elf va .bin san sang.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_release)
