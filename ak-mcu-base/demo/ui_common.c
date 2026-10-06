/**
 * Chip- and kernel-independent parts of the demo UI: menu, layout helpers,
 * random numbers. Shared by the firmware and the host renderer.
 */
#include "ui.h"

/*----------------------------------------------------------------------------
 * helpers
 *--------------------------------------------------------------------------*/
static uint32_t rng_state = 0x2545F491UL;

uint32_t ui_rand(void) {
	/* xorshift32 */
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/* Title bar: inverted strip of 9 pixels with a left and a right text. */
void ui_title(const char* left, const char* right) {
	gfx_text(2, 1, left, 1);
	if (right) {
		gfx_text(GFX_W - 2 - gfx_text_width(right, 1), 1, right, 1);
	}
	gfx_invert(0, 0, GFX_W, 9);
}

/* Bottom line: what the three buttons do, each above its button. */
void ui_footer(const char* b1, const char* b2, const char* b3) {
	gfx_hline(0, 54, GFX_W, 1);
	if (b1) {
		gfx_text(1, 56, b1, 1);
	}
	if (b2) {
		gfx_text((GFX_W - gfx_text_width(b2, 1)) / 2, 56, b2, 1);
	}
	if (b3) {
		gfx_text(GFX_W - 1 - gfx_text_width(b3, 1), 56, b3, 1);
	}
}

/*----------------------------------------------------------------------------
 * menu
 *--------------------------------------------------------------------------*/
static const ui_screen_t* const menu_items[] = {
	&scr_clock,
	&scr_snake,
	&scr_flappy,
	&scr_system,
};

#define MENU_NUM	((uint8_t)(sizeof(menu_items) / sizeof(menu_items[0])))

static uint8_t menu_sel;

void menu_enter(void) {
}

const ui_screen_t* menu_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		menu_sel = (uint8_t)((menu_sel + 1) % MENU_NUM);
		ui_beep(1800, 15);
	}
	else if (btn == KIT_BTN_2) {
		menu_sel = (uint8_t)((menu_sel + MENU_NUM - 1) % MENU_NUM);
		ui_beep(1800, 15);
	}
	else if (btn == KIT_BTN_3) {
		ui_beep(2400, 40);
		return menu_items[menu_sel];
	}
	return 0;
}

void menu_frame(uint32_t now_ms) {
	(void)now_ms;

	gfx_clear();
	ui_title("AK MCU KIT", "demo");
	for (uint8_t i = 0; i < MENU_NUM; i++) {
		int y = 12 + i * 10;

		gfx_text(10, y + 1, menu_items[i]->name, 1);
		if (i == menu_sel) {
			gfx_text(2, y + 1, ">", 1);
			gfx_invert(0, y, GFX_W, 9);
		}
	}
	ui_footer("DOWN", "UP", "OPEN");
}
