# ==============================================================================
# ak-mcu-base - build system
#
#   Host (tests + simulator):
#     cmake -S . -B build/host && cmake --build build/host && ctest --test-dir build/host
#
#   STM32L151 (boot + app + OTA image):
#     cmake -S . -B build/stm32l151 -DAK_PORT=stm32l151 \
#           -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
#     cmake --build build/stm32l151
#
#   Shortcut: make test / make sim / make stm32 (Makefile in this folder).
# ==============================================================================
cmake_minimum_required(VERSION 3.16)

set(AK_PORT "host" CACHE STRING "Target port: host | stm32l151")

project(ak_mcu_base C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# App version: edit here or pass -DAPP_VERSION=1.2.3
set(APP_VERSION "1.2.0" CACHE STRING "App version major.minor.patch")
set(APP_BUILD "0" CACHE STRING "Build number (incremented by CI)")
set(BOOT_VERSION "1.2.0" CACHE STRING "Bootloader version")

string(REPLACE "." ";" _av ${APP_VERSION})
list(GET _av 0 APP_VER_MAJOR)
list(GET _av 1 APP_VER_MINOR)
list(GET _av 2 APP_VER_PATCH)
string(REPLACE "." ";" _bv ${BOOT_VERSION})
list(GET _bv 0 BOOT_VER_MAJOR)
list(GET _bv 1 BOOT_VER_MINOR)
list(GET _bv 2 BOOT_VER_PATCH)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------------------
# Chip-independent components
# ------------------------------------------------------------------------------
set(KERNEL_SRC
	${ROOT}/kernel/src/fsm.c
	${ROOT}/kernel/src/tsm.c
	${ROOT}/kernel/src/message.c
	${ROOT}/kernel/src/task.c
	${ROOT}/kernel/src/timer.c
)

set(COMMON_SRC
	${ROOT}/common/xprintf.c
	${ROOT}/common/crc.c
)

set(FW_SRC
	${ROOT}/services/fw/fw_image.c
	${ROOT}/services/fw/fw_update.c
	${ROOT}/services/fw/fw_proto.c
	${ROOT}/services/fw/boot_ctrl.c
)

set(SYS_SRC
	${ROOT}/services/sys/crash_log.c
)

set(BOOT_SRC
	${ROOT}/boot/boot_core.c
	${ROOT}/boot/boot_main.c
)

set(APP_SRC
	${ROOT}/app/main.c
	${ROOT}/app/task_list.c
	${ROOT}/app/task_system.c
	${ROOT}/app/task_console.c
	${ROOT}/app/task_fw.c
)

set(BASE_INC
	${ROOT}/kernel/inc
	${ROOT}/common
	${ROOT}/hal
	${ROOT}/services/fw
	${ROOT}/services/sys
	${ROOT}/boot
)

set(VERSION_DEFS
	APP_VER_MAJOR=${APP_VER_MAJOR}
	APP_VER_MINOR=${APP_VER_MINOR}
	APP_VER_PATCH=${APP_VER_PATCH}
	APP_VER_BUILD=${APP_BUILD}
	BOOT_VER_MAJOR=${BOOT_VER_MAJOR}
	BOOT_VER_MINOR=${BOOT_VER_MINOR}
	BOOT_VER_PATCH=${BOOT_VER_PATCH}
)

if(AK_PORT STREQUAL "host")
	include(${ROOT}/port/host/port.cmake)
elseif(AK_PORT STREQUAL "stm32l151")
	include(${ROOT}/port/stm32l151/port.cmake)
else()
	message(FATAL_ERROR "Unknown AK_PORT: ${AK_PORT}")
endif()
