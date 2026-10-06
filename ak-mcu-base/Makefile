# Shortcuts around CMake.
#   make test     host unit tests + simulator end-to-end test
#   make sim      build and run the simulator (bootloader + app)
#   make stm32    build boot.elf/.bin, app.elf/.bin, app.img for STM32L151
#   make clean

BUILD_HOST  := build/host
BUILD_STM32 := build/stm32l151

.PHONY: all host test sim stm32 clean

all: test stm32

host:
	cmake -S . -B $(BUILD_HOST)
	cmake --build $(BUILD_HOST) -j

test: host
	cd $(BUILD_HOST) && ctest --output-on-failure

sim: host
	$(BUILD_HOST)/ak_sim --flash $(BUILD_HOST)/ak_sim_flash.bin

stm32:
	cmake -S . -B $(BUILD_STM32) -DAK_PORT=stm32l151 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
	cmake --build $(BUILD_STM32) -j

clean:
	rm -rf build .pio
