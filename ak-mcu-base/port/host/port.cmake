# Host port: unit tests + ak_sim simulator
set(HOST_DIR ${ROOT}/port/host)
set(HOST_WARN -Wall -Wextra -Wno-unused-parameter -Werror)

enable_testing()

# --- kernel tests (own task table in tests/kernel) ---
add_executable(test_kernel
	${ROOT}/tests/test_kernel.c
	${KERNEL_SRC} ${COMMON_SRC}
	${HOST_DIR}/port_host.c
)
target_include_directories(test_kernel PRIVATE ${ROOT}/tests/kernel ${ROOT}/tests ${BASE_INC} ${HOST_DIR})
target_compile_options(test_kernel PRIVATE ${HOST_WARN} -fsanitize=address,undefined -g)
target_link_options(test_kernel PRIVATE -fsanitize=address,undefined)
add_test(NAME kernel COMMAND test_kernel)

# --- firmware update + bootloader tests ---
add_executable(test_fw
	${ROOT}/tests/test_fw.c
	${COMMON_SRC} ${FW_SRC} ${BOOT_SRC}
	${HOST_DIR}/port_host.c
)
target_include_directories(test_fw PRIVATE ${ROOT}/tests ${BASE_INC} ${HOST_DIR})
target_compile_definitions(test_fw PRIVATE AK_SIM ${VERSION_DEFS})
target_compile_options(test_fw PRIVATE ${HOST_WARN} -fsanitize=address,undefined -g)
target_link_options(test_fw PRIVATE -fsanitize=address,undefined)
add_test(NAME fw COMMAND test_fw)

# --- ak_sim: bootloader + app running on the PC ---
add_executable(ak_sim
	${HOST_DIR}/sim_main.c
	${HOST_DIR}/port_host.c
	${KERNEL_SRC} ${COMMON_SRC} ${FW_SRC} ${BOOT_SRC} ${APP_SRC}
)
target_include_directories(ak_sim PRIVATE ${ROOT}/app ${BASE_INC} ${HOST_DIR})
target_compile_definitions(ak_sim PRIVATE AK_SIM ${VERSION_DEFS})
target_compile_options(ak_sim PRIVATE ${HOST_WARN} -g)

# --- end-to-end: tools/ak_fw.py flashes ak_sim over pipes ---
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
	add_test(NAME sim_ota
		COMMAND ${Python3_EXECUTABLE} ${ROOT}/tests/test_sim_ota.py
			--sim $<TARGET_FILE:ak_sim> --tools ${ROOT}/tools
			--workdir ${CMAKE_CURRENT_BINARY_DIR})
endif()
