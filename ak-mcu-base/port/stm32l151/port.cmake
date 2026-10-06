# Port STM32L151CB: builds boot.elf/.bin, app.elf/.bin and app.img (OTA image).
set(PORT_DIR ${ROOT}/port/stm32l151)

# SPL + CMSIS are reused from the parent ak-base-kit-pio tree (not duplicated).
set(STM32L1_LIB_DIR ${ROOT}/../sources/application/platform/stm32l/Libraries
	CACHE PATH "Directory containing STM32L1xx_StdPeriph_Driver and CMSIS")
if(NOT EXISTS ${STM32L1_LIB_DIR}/STM32L1xx_StdPeriph_Driver)
	message(FATAL_ERROR "SPL not found: ${STM32L1_LIB_DIR} (set -DSTM32L1_LIB_DIR=...)")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SPL_DIR ${STM32L1_LIB_DIR}/STM32L1xx_StdPeriph_Driver)
set(SPL_SRC
	${SPL_DIR}/src/misc.c
	${SPL_DIR}/src/stm32l1xx_rcc.c
	${SPL_DIR}/src/stm32l1xx_gpio.c
	${SPL_DIR}/src/stm32l1xx_usart.c
	${SPL_DIR}/src/stm32l1xx_flash.c
	${SPL_DIR}/src/stm32l1xx_iwdg.c
)

set(PORT_INC
	${PORT_DIR}
	${SPL_DIR}/inc
	${STM32L1_LIB_DIR}/CMSIS/Device/ST/STM32L1xx/Include
	${STM32L1_LIB_DIR}/CMSIS/Include
)

set(MCU_FLAGS -mcpu=cortex-m3 -mthumb -mfloat-abi=soft)
set(MCU_DEFS USE_STDPERIPH_DRIVER STM32L1XX_MD)
set(MCU_WARN -Wall -Wextra -Wno-unused-parameter)

# SPL compiled once, without our warning set (vendor code).
add_library(spl STATIC ${SPL_SRC})
target_include_directories(spl PUBLIC ${PORT_INC})
target_compile_definitions(spl PUBLIC ${MCU_DEFS})
target_compile_options(spl PRIVATE ${MCU_FLAGS} -Os -g -ffunction-sections -fdata-sections)

function(ak_firmware name ldscript)
	cmake_parse_arguments(FW "" "" "SOURCES;DEFS;INCS" ${ARGN})
	add_executable(${name} ${FW_SOURCES})
	set_target_properties(${name} PROPERTIES SUFFIX ".elf")
	target_include_directories(${name} PRIVATE ${FW_INCS} ${BASE_INC} ${PORT_INC})
	target_compile_definitions(${name} PRIVATE ${MCU_DEFS} ${VERSION_DEFS} ${FW_DEFS})
	target_compile_options(${name} PRIVATE ${MCU_FLAGS} -Os -g -ffunction-sections -fdata-sections ${MCU_WARN} -Werror)
	target_link_options(${name} PRIVATE ${MCU_FLAGS}
		-T${PORT_DIR}/${ldscript} -L${PORT_DIR}
		-nostartfiles --specs=nano.specs
		-Wl,--gc-sections -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${name}.map -Wl,--print-memory-usage)
	target_link_libraries(${name} PRIVATE spl c gcc)
	set_target_properties(${name} PROPERTIES LINK_DEPENDS "${PORT_DIR}/${ldscript};${PORT_DIR}/sections.ld")
endfunction()

ak_firmware(boot boot.ld
	SOURCES
		${PORT_DIR}/startup.c ${PORT_DIR}/port_stm32l151.c ${PORT_DIR}/system_stm32l1xx.c
		${COMMON_SRC} ${FW_SRC} ${BOOT_SRC}
	DEFS AK_BOOTLOADER
)
add_custom_command(TARGET boot POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:boot> ${CMAKE_CURRENT_BINARY_DIR}/boot.bin
	COMMENT "boot.bin")

ak_firmware(app app.ld
	SOURCES
		${PORT_DIR}/startup.c ${PORT_DIR}/port_stm32l151.c ${PORT_DIR}/system_stm32l1xx.c
		${PORT_DIR}/fw_header.c
		${KERNEL_SRC} ${COMMON_SRC} ${FW_SRC} ${APP_SRC}
	INCS ${ROOT}/app
)
# Fill header CRC/size into app.bin -> app.img, and patch .fw_header in app.elf
# so that flashing the .elf over SWD also yields a valid image.
add_custom_command(TARGET app POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app> ${CMAKE_CURRENT_BINARY_DIR}/app.bin
	COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/mkimage.py patch ${CMAKE_CURRENT_BINARY_DIR}/app.bin
		-o ${CMAKE_CURRENT_BINARY_DIR}/app.img --elf $<TARGET_FILE:app> --objcopy ${CMAKE_OBJCOPY}
	COMMENT "app.img (patched header)")
