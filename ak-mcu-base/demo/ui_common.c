/**
 * Chip- and kernel-independent parts of the demo UI: menu, layout helpers,
 * random numbers. Shared by the firmware and the host renderer.
 */
#include "ui.h"

/*----------------------------------------------------------------------------
 * helpers
 *--------------------------------------------------------------------------*/
uint8_t ui_autoplay;

static uint32_t rng_state = 0x2545F491UL;

uint32_t ui_rand(void) {
	/* xorshift32 */
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/* 127 * sin(2 * pi * i / 256) */
static const int8_t sin_tbl[256] = {
	   0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
	  49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
	  90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
	 117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
	 127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
	 117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
	  90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
	  49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
	   0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
	 -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
	 -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
	-117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
	-127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
	-117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
	 -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
	 -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
};

int ui_sin(uint8_t a) {
	return sin_tbl[a];
}

int ui_cos(uint8_t a) {
	return sin_tbl[(uint8_t)(a + 64)];
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
		/* centred in what the two outer labels leave free */
		int left = b1 ? 1 + gfx_text_width(b1, 1) : 0;
		int right = b3 ? GFX_W - 1 - gfx_text_width(b3, 1) : GFX_W;

		gfx_text(left + (right - left - gfx_text_width(b2, 1)) / 2, 56, b2, 1);
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
	&scr_dino,
	&scr_cube,
	&scr_maze,
	&scr_music,
	&scr_video,
	&scr_weather,
	&scr_system,
};

#define MENU_NUM	((uint8_t)(sizeof(menu_items) / sizeof(menu_items[0])))

#define MENU_LINES	(4)

static uint8_t menu_sel;
static uint8_t menu_top;			/* first entry shown */

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
	if (menu_sel < menu_top) {
		menu_top = menu_sel;
	}
	else if (menu_sel >= menu_top + MENU_LINES) {
		menu_top = (uint8_t)(menu_sel - MENU_LINES + 1);
	}
	for (uint8_t line = 0; line < MENU_LINES && menu_top + line < MENU_NUM; line++) {
		uint8_t i = (uint8_t)(menu_top + line);
		int y = 12 + line * 10;

		gfx_text(10, y + 1, menu_items[i]->name, 1);
		if (i == menu_sel) {
			gfx_text(2, y + 1, ">", 1);
			gfx_invert(0, y, GFX_W - 4, 9);
		}
	}
	/* scroll bar: where the window sits in the list */
	gfx_vline(GFX_W - 2, 12, MENU_LINES * 10 - 1, 1);
	gfx_fill(GFX_W - 3, 12 + menu_top * (MENU_LINES * 10 - 1) / MENU_NUM, 3,
			 MENU_LINES * (MENU_LINES * 10 - 1) / MENU_NUM + 1, 1);
	ui_footer("DOWN", "UP", "OPEN");
}
