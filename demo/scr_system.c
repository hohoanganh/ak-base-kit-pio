/**
 * System monitor: what the kernel is doing while the demo runs.
 *   B1: beep
 */
#include <string.h>

#include "ui.h"
#include "hal.h"
#include "message.h"
#include "timer.h"
#include "crash_log.h"

/* "label a/b" at (x, y) */
static void pair(int x, int y, const char* label, uint32_t a, uint32_t b) {
	char buf[12];

	x = gfx_text(x, y, label, 1);
	x = gfx_text(x + 3, y, gfx_utoa(buf, a, 1), 1);
	x = gfx_text(x, y, "/", 1);
	gfx_text(x, y, gfx_utoa(buf, b, 1), 1);
}

static void value(int y, const char* label, uint32_t v, const char* unit) {
	char buf[12];
	int x = gfx_text(0, y, label, 1);

	x = gfx_text(x + 3, y, gfx_utoa(buf, v, 1), 1);
	if (unit) {
		gfx_text(x + 2, y, unit, 1);
	}
}

static void system_enter(void) {
}

static void system_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		ui_beep(2000, 60);
	}
}

static void system_frame(uint32_t now_ms) {
	gfx_clear();
	ui_title("SYSTEM", hal_board_name());

	value(10, "uptime", now_ms / 1000U, "s");
	/* message pools: in use now / most ever in use */
	pair(0, 19, "pure", get_pure_msg_pool_used(), get_pure_msg_pool_used_max());
	pair(66, 19, "tmr", get_timer_msg_pool_used(), get_timer_msg_pool_used_max());
	value(28, "RAM never used", hal_stack_unused(), "B");
	value(37, "crash records", crash_log_count(), 0);
	/* cost of the previous frame: pages sent to the display, time it took */
	pair(0, 46, "frame", ui_last_frame_ms, UI_FRAME_MS);
	pair(66, 46, "pages", ui_last_pages, KIT_LCD_PAGES);

	ui_footer("BEEP", 0, "hold:MENU");
}

const ui_screen_t scr_system = { "System monitor", system_enter, system_key, system_frame, 0 };
