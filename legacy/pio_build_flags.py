# pio_build_flags.py
# Co bien dich RIENG TUNG NGON NGU + co cho buoc LINK. Khong the dat trong
# build_flags cua platformio.ini vi build_flags chi vao CCFLAGS (ap cho ca C
# lan C++), con LINKCOM khong doc CCFLAGS (kinh nghiem tu du an truoc:
# crtbegin.o van bi link du build_flags co -nostartfiles -> "multiple
# definition of __dso_handle").
#
# - CFLAGS  -std=gnu99            : Makefile goc dung -std=c99 (gnu99 de giu
#                                   cac extension asm/typeof ma SPL hay dung).
# - CXXFLAGS -std=gnu++11         : Makefile goc dung -std=c++11.
#            -fno-rtti -fno-exceptions -fno-use-cxa-atexit : khop CPPFLAGS goc,
#                                   bat buoc voi mini_cpp.cpp (tu cai dat
#                                   new/delete/guard, khong co runtime C++ day du).
# - LINKFLAGS:
#   * -nostartfiles     : startup tu viet (sys_ctrl.s/stm32l.c + mini_cpp.cpp
#                         dinh nghia reset_handler/__dso_handle rieng) - phai
#                         chan GCC link crtbegin.o/crtend.o mac dinh.
#   * --specs=nano.specs: newlib-nano + libstdc++_nano, khop *_nano trong
#                         Makefile goc; can de vua vung flash (boot chi 8K).
#   * --specs=nosys.specs: stub rong cho syscall newlib (_exit/_write/_sbrk...)
#                         khi libc keo theo abort()/malloc.
#   * -Os               : toi uu size o buoc link.
#   * -Wl,-z,max-page-size=4 : BAT BUOC voi env:app. Mac dinh ld can le segment
#                         theo trang 64K, nen segment PT_LOAD cua app (dat o
#                         0x08003000) bi keo lui p_paddr ve 0x08000000 va nuot
#                         them 12K rac o dau. `pio run -t upload` nap bang
#                         openocd `program firmware.elf`, ma openocd doc
#                         PROGRAM HEADER chu khong doc section -> se ghi de
#                         header ELF len vung bootloader 0x08000000 va xoa BSF
#                         -> nap app xong board treo ngay. Ep max-page-size=4
#                         de segment bat dau dung 0x08003000.
#
# CO BIEN DICH PHAI VAO CA `projenv`. Script nay la POST script: luc no chay,
# PlatformIO da tach `projenv` (moi truong bien dich code trong src_dir) ra
# khoi `env`. Chi Append vao `env` thi co LINK van co tac dung (buoc link dung
# `env`), nhung CFLAGS/CXXFLAGS/CPPDEFINES KHONG toi duoc file nguon nao - den
# v1.1.1, -std=gnu99 / -std=gnu++11 / -fno-use-cxa-atexit chua tung duoc dung,
# code bien dich bang mac dinh cua GCC 9 (gnu11 / gnu++14). Da kiem bang
# `pio run -v`, 24/09/2026.
Import("env", "projenv")

for _e in (env, projenv):
    _e.Append(CFLAGS=["-std=gnu99"])
    _e.Append(CXXFLAGS=["-std=gnu++11", "-fno-rtti", "-fno-exceptions",
                        "-fno-use-cxa-atexit"])
env.Append(LINKFLAGS=["-nostartfiles", "--specs=nano.specs",
                      "--specs=nosys.specs", "-Os",
                      "-Wl,-z,max-page-size=4", "-Wl,--nmagic"])
# Makefile goc link kem libm (xprintf co the dung float).
env.Append(LIBS=["m"])

# Version: NGUON DUY NHAT la -DAPP_VERSION="x.y.z" trong platformio.ini.
# pio_copy_release.py dung no dat ten file release; o day tach ra 3 so de
# app.h dung APP_VER {major, minor, patch, 0} cho app_info (in luc khoi dong,
# lenh shell `ver`). Ban cu go cung APP_VER {0, 0, 0, 3} trong app.h: doi
# APP_VERSION chi doi ten file, board van bao 0.0.0.3.
import re


def _app_version(env):
    for item in env.get("CPPDEFINES", []):
        if isinstance(item, (list, tuple)) and len(item) == 2 and item[0] == "APP_VERSION":
            m = re.search(r"(\d+)\.(\d+)\.(\d+)", str(item[1]))
            if m:
                return [int(x) for x in m.groups()]
    return None


_ver = _app_version(env)
if _ver:
    for _e in (env, projenv):
        _e.Append(CPPDEFINES=[("APP_VER_MAJOR", _ver[0]),
                              ("APP_VER_MINOR", _ver[1]),
                              ("APP_VER_PATCH", _ver[2])])
