/**
 ******************************************************************************
 * @brief:  App entry: init AK kernel, task tables, run the scheduler.
 ******************************************************************************
**/

#include <string.h>

#include "ak.h"
#include "task.h"
#include "timer.h"
#include "message.h"

#include "hal.h"
#include "ak_log.h"
#include "fw_image.h"

#include "app.h"
#include "task_list.h"

static fw_version_t running_version = { APP_VER_MAJOR, APP_VER_MINOR, APP_VER_PATCH, 0, APP_VER_BUILD };

const fw_version_t* app_version(void) {
	return &running_version;
}

/* Called by SysTick (MCU) / host clock: drives the kernel timer. */
void hal_tick_hook(uint32_t elapsed_ms) {
	task_entry_interrupt();
	timer_tick(elapsed_ms);
	task_exit_interrupt();
}

int app_main(void) {
	fw_image_hdr_t hdr;

	if (hal_flash_read(FLASH_PART_APP, 0, &hdr, sizeof(hdr)) == HAL_FLASH_OK &&
			fw_image_check_hdr(&hdr, hal_flash_info(FLASH_PART_APP)->size) == FW_OK) {
		memcpy(&running_version, &hdr.version, sizeof(running_version));
	}

	LOG_I("APP", "ak-mcu-base app v%d.%d.%d build %u, board %s\n",
		  running_version.major, running_version.minor, running_version.patch,
		  running_version.build, hal_board_name());

	task_init();
	task_create(app_task_table);
	task_polling_create(app_task_polling_table);

	task_post_pure_msg(TASK_SYSTEM_ID, SYSTEM_SIG_INIT);
	task_post_pure_msg(TASK_CONSOLE_ID, CONSOLE_SIG_INIT);

	return task_run();
}

#if !defined(AK_SIM)
int main(void) {
	return app_main();
}
#endif
