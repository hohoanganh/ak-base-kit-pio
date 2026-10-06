#include "ak.h"
#include "timer.h"

#include "hal.h"
#include "ak_log.h"

#include "app.h"
#include "task_list.h"

void task_system(ak_msg_t* msg) {
	switch (msg->sig) {
	case SYSTEM_SIG_INIT:
		hal_wdt_start(APP_WDT_TIMEOUT_MS);
		timer_set(TASK_SYSTEM_ID, SYSTEM_SIG_HEARTBEAT, APP_HEARTBEAT_MS, TIMER_PERIODIC);
		LOG_I("SYS", "watchdog %d ms, heartbeat %d ms\n", APP_WDT_TIMEOUT_MS, APP_HEARTBEAT_MS);
		break;

	case SYSTEM_SIG_HEARTBEAT:
		/* Kick from a task, not an ISR: a task hogging the CPU delays the
		 * heartbeat and the watchdog resets the chip. */
		hal_wdt_kick();
		hal_led_toggle();
		break;

	default:
		break;
	}
}
