# PlatformIO extra script for ak-mcu-base (STM32L151).
#  - link flags: no startup files, newlib-nano, search path for sections.ld
#  - env:app post-build: patch image header (size + CRC) into firmware.elf and
#    write app.img next to it (same steps as port/stm32l151/port.cmake)
Import("env")

import os

proj = env.subst("$PROJECT_DIR")

env.Append(LINKFLAGS=[
    "-mcpu=cortex-m3", "-mthumb", "-mfloat-abi=soft",
    "-nostartfiles", "--specs=nano.specs",
    "-Wl,--gc-sections", "-Wl,--print-memory-usage",
    "-L" + os.path.join(proj, "port", "stm32l151"),
])

# SPL sources from the parent tree (outside src_dir -> built explicitly)
spl_src = os.path.normpath(os.path.join(
    proj, "..", "sources", "application", "platform", "stm32l", "Libraries",
    "STM32L1xx_StdPeriph_Driver", "src"))
env.BuildSources(os.path.join("$BUILD_DIR", "spl"), spl_src, src_filter=[
    "-<*>", "+<misc.c>", "+<stm32l1xx_rcc.c>", "+<stm32l1xx_gpio.c>",
    "+<stm32l1xx_usart.c>", "+<stm32l1xx_flash.c>", "+<stm32l1xx_iwdg.c>",
])

if env.subst("$PIOENV") == "app":
    def patch_image(source, target, env):
        elf = target[0].get_abspath()
        build = env.subst("$BUILD_DIR")
        raw = os.path.join(build, "app_raw.bin")
        img = os.path.join(build, "app.img")
        objcopy = env.subst("$OBJCOPY")
        env.Execute('"%s" -O binary "%s" "%s"' % (objcopy, elf, raw))
        env.Execute('"$PYTHONEXE" "%s" patch "%s" -o "%s" --elf "%s" --objcopy "%s"' % (
            os.path.join(proj, "tools", "mkimage.py"), raw, img, elf, objcopy))

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", patch_image)
