# pio_build_flags.py
# Co bien dich RIENG TUNG NGON NGU + co cho buoc LINK. Khong the dat trong
# build_flags cua platformio.ini vi build_flags chi vao CCFLAGS (ap cho ca C
# lan C++), con LINKCOM khong doc CCFLAGS (kinh nghiem tu Smart-PDU-firmware:
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
Import("env")

env.Append(CFLAGS=["-std=gnu99"])
env.Append(CXXFLAGS=["-std=gnu++11", "-fno-rtti", "-fno-exceptions",
                     "-fno-use-cxa-atexit"])
env.Append(LINKFLAGS=["-nostartfiles", "--specs=nano.specs",
                      "--specs=nosys.specs", "-Os"])
# Makefile goc link kem libm (xprintf co the dung float).
env.Append(LIBS=["m"])
