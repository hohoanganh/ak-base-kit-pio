/**
 ******************************************************************************
 * @brief:  ak_sim - whole-system simulator (bootloader + app) on a PC.
 *
 *   ./ak_sim [--flash file.bin] [--fresh]
 *
 *  - Console = stdin/stdout (text shell + fw protocol, like the real UART).
 *  - Flash + NVM saved to a file (default ak_sim_flash.bin) on every reset
 *    and exit -> downloaded firmware persists across runs.
 *  - Reset = restart boot_main(). "Jump to app" = call the app_main() built
 *    into ak_sim (the APP image is only verified; ARM code cannot run here).
 ******************************************************************************
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "hal.h"
#include "port_host.h"

extern void boot_main(void);
extern int app_main(void);

static jmp_buf reset_jmp;
static const char* flash_path = "ak_sim_flash.bin";

static void save_flash(void) {
	host_flash_save(flash_path);
}

static void on_reset(void) {
	save_flash();
	longjmp(reset_jmp, 1);
}

static void on_jump(uint32_t vector_addr) {
	(void)vector_addr;
	save_flash();
	app_main();
	exit(0);
}

int main(int argc, char** argv) {
	int fresh = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--flash") == 0 && i + 1 < argc) {
			flash_path = argv[++i];
		}
		else if (strcmp(argv[i], "--fresh") == 0) {
			fresh = 1;
		}
		else {
			fprintf(stderr, "usage: %s [--flash file.bin] [--fresh]\n", argv[0]);
			return 2;
		}
	}

	host_reset_state(0xFF);
	if (fresh || host_flash_load(flash_path) != 0) {
		host_reset_state(0xFF);
		fprintf(stderr, "[sim] new flash: %s\n", flash_path);
	}

	host_reset_handler = on_reset;
	host_jump_handler = on_jump;
	host_console_use_stdio(1);
	atexit(save_flash);

	if (setjmp(reset_jmp)) {
		fprintf(stderr, "[sim] reset\n");
	}

	hal_init();
	boot_main();
	return 0;
}
