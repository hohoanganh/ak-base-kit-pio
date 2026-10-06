/**
 * Demo UI on host: the screens draw into the frame buffer exactly as on the
 * kit; this test plays them with scripted buttons, checks the game logic and
 * writes what the display would show as PBM pictures (argv[1] = folder).
 *
 *   test_demo <folder> record   instead of the tests: play scripted scenes and
 *                               write every frame to <folder>/<scene>.frames
 *                               (1024 bytes per frame, display page order).
 *                               tools/demo_gif.py turns them into GIFs.
 */
#include <stdio.h>
#include <string.h>

#include "ak.h"
#include "task.h"
#include "timer.h"
#include "hal.h"
#include "ui.h"
#include "port_host.h"
#include "tiny_test.h"

/*----------------------------------------------------------------------------
 * the kit, emulated
 *--------------------------------------------------------------------------*/
static uint8_t panel[KIT_LCD_PAGES][KIT_LCD_W];		/* what the display shows */
static int pages_written;
static uint8_t rtc_present, rtc_h, rtc_m, rtc_s;
static uint32_t rtc_base_ms;		/* now_ms when rtc_h:m:s was set */
static uint32_t now_ms;
static int beeps;

uint8_t ui_last_pages;
uint16_t ui_last_frame_ms;

uint8_t kit_init(void) { return 1; }
uint8_t kit_buttons(void) { return 0; }
void kit_buzzer(uint16_t freq_hz) { (void)freq_hz; }

uint8_t kit_lcd_write_page(uint8_t page, const uint8_t* data) {
	memcpy(panel[page], data, KIT_LCD_W);
	pages_written++;
	return 1;
}

uint8_t kit_rtc_get(uint8_t* hh, uint8_t* mm, uint8_t* ss) {
	uint32_t t;

	if (!rtc_present) {
		return 0;
	}
	t = ((uint32_t)rtc_h * 3600UL + (uint32_t)rtc_m * 60UL + rtc_s + (now_ms - rtc_base_ms) / 1000UL) % 86400UL;
	*hh = (uint8_t)(t / 3600UL);
	*mm = (uint8_t)((t / 60UL) % 60UL);
	*ss = (uint8_t)(t % 60UL);
	return 1;
}

uint8_t kit_rtc_set(uint8_t hh, uint8_t mm, uint8_t ss) {
	if (!rtc_present) {
		return 0;
	}
	rtc_h = hh; rtc_m = mm; rtc_s = ss;
	rtc_base_ms = now_ms;
	return 1;
}

void ui_beep(uint16_t freq_hz, uint16_t ms) {
	(void)freq_hz;
	(void)ms;
	beeps++;
}

/*----------------------------------------------------------------------------
 * helpers
 *--------------------------------------------------------------------------*/
static const char* out_dir;

static void frames(const ui_screen_t* s, int n) {
	while (n--) {
		now_ms += UI_FRAME_MS;
		host_advance_ms(UI_FRAME_MS);
		if (s) {
			s->frame(now_ms);
		}
		else {
			menu_frame(now_ms);
		}
		gfx_flush(0);
	}
}

/*----------------------------------------------------------------------------
 * recording: scripted scenes, one file of raw frames each
 *--------------------------------------------------------------------------*/
static FILE* rec_file;

static void rec_open(const char* scene) {
	char path[300];

	snprintf(path, sizeof(path), "%s/%s.frames", out_dir, scene);
	rec_file = fopen(path, "wb");
}

static void rec_close(void) {
	if (rec_file) {
		fclose(rec_file);
		rec_file = 0;
	}
}

/* n frames of screen s (0 = menu), each one written out */
static void play(const ui_screen_t* s, int n) {
	while (n--) {
		frames(s, 1);
		if (rec_file) {
			fwrite(panel, 1, sizeof(panel), rec_file);
		}
	}
}

static void menu_press(uint8_t btn, int hold_frames) {
	menu_key(btn);
	play(0, hold_frames);
}

static void record(void) {
	rtc_present = 1; rtc_h = 10; rtc_m = 9; rtc_s = 52;
	ui_autoplay = 1;

	/* the tour: menu, clock, both games playing themselves, system monitor */
	rec_open("tour");
	menu_enter();
	play(0, 16);
	scr_clock.enter();
	play(&scr_clock, 70);
	scr_clock.key(KIT_BTN_1);				/* set hours */
	play(&scr_clock, 14);
	scr_clock.key(KIT_BTN_2);
	play(&scr_clock, 14);
	scr_clock.key(KIT_BTN_1);
	scr_clock.key(KIT_BTN_1);
	play(&scr_clock, 16);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_snake.enter();
	play(&scr_snake, 360);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_flappy.enter();
	play(&scr_flappy, 300);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_system.enter();
	play(&scr_system, 50);
	play(0, 6);
	menu_press(KIT_BTN_1, 10);				/* wraps to the first entry: the GIF loops cleanly */
	rec_close();

	rec_open("snake");
	scr_snake.enter();
	play(&scr_snake, 520);
	rec_close();

	rec_open("flappy");
	scr_flappy.enter();
	play(&scr_flappy, 420);
	rec_close();

	rec_open("clock");
	rtc_h = 23; rtc_m = 59; rtc_s = 55;
	rtc_base_ms = now_ms;
	scr_clock.enter();
	play(&scr_clock, 200);					/* rolls over midnight */
	rec_close();
}

static void snapshot(const char* name) {
	char path[300];
	FILE* f;

	if (!out_dir) {
		return;
	}
	snprintf(path, sizeof(path), "%s/%s.pbm", out_dir, name);
	f = fopen(path, "w");
	if (!f) {
		return;
	}
	fprintf(f, "P1\n%d %d\n", KIT_LCD_W, KIT_LCD_H);
	for (int y = 0; y < KIT_LCD_H; y++) {
		for (int x = 0; x < KIT_LCD_W; x++) {
			fputc((panel[y >> 3][x] >> (y & 7)) & 1 ? '1' : '0', f);
		}
		fputc('\n', f);
	}
	fclose(f);
}

static int lit_pixels(void) {
	int n = 0;

	for (int y = 0; y < KIT_LCD_H; y++) {
		for (int x = 0; x < KIT_LCD_W; x++) {
			n += (panel[y >> 3][x] >> (y & 7)) & 1;
		}
	}
	return n;
}

/*----------------------------------------------------------------------------
 * tests
 *--------------------------------------------------------------------------*/
static void test_gfx_flush_only_changes(void) {
	gfx_clear();
	gfx_flush(1);
	pages_written = 0;
	CHECK_EQ(gfx_flush(0), 0);				/* nothing changed: nothing sent */
	gfx_pixel(5, 20, 1);					/* page 2 */
	gfx_pixel(100, 63, 1);					/* page 7 */
	CHECK_EQ(gfx_flush(0), 2);
	CHECK_EQ(pages_written, 2);
	CHECK_EQ(lit_pixels(), 2);
	CHECK_EQ(gfx_flush(0), 0);
	/* outside the screen: ignored, no memory touched */
	gfx_pixel(-1, 0, 1); gfx_pixel(128, 0, 1); gfx_pixel(0, 64, 1); gfx_fill(120, 60, 20, 20, 1);
	CHECK_EQ(gfx_get(127, 63), 1);
	CHECK_EQ(gfx_get(128, 63), 0);
}

static void test_text(void) {
	char buf[12];

	CHECK(strcmp(gfx_utoa(buf, 0, 1), "0") == 0);
	CHECK(strcmp(gfx_utoa(buf, 7, 2), "07") == 0);
	CHECK(strcmp(gfx_utoa(buf, 4294967295UL, 1), "4294967295") == 0);
	CHECK_EQ(gfx_text_width("AB", 1), 11);
	CHECK_EQ(gfx_text_width("AB", 2), 22);
	gfx_clear();
	gfx_text_center(0, "The quick brown fox", 1);
	gfx_text_center(9, "JUMPS OVER 0123456789", 1);
	gfx_text_center(18, "!\"#$%&'()*+,-./:;<=>?", 1);
	gfx_text_center(27, "@[\\]^_`{|}~ lazy dog", 1);
	gfx_text_center(38, "Big 42", 2);
	gfx_flush(0);
	snapshot("0_font");
}

static void test_menu(void) {
	menu_enter();
	frames(0, 1);
	snapshot("1_menu");
	CHECK(menu_key(KIT_BTN_1) == 0);			/* down: Snake */
	frames(0, 1);
	CHECK(menu_key(KIT_BTN_3) == &scr_snake);
	CHECK(menu_key(KIT_BTN_2) == 0);			/* up: back to the clock */
	CHECK(menu_key(KIT_BTN_2) == 0);			/* up again: wraps to the last entry */
	CHECK(menu_key(KIT_BTN_3) == &scr_system);
	CHECK(menu_key(KIT_BTN_1) == 0);			/* down: wraps to the first */
	CHECK(menu_key(KIT_BTN_3) == &scr_clock);
}

static void test_clock(void) {
	/* with an RTC: shows its time, setting writes it back */
	rtc_present = 1; rtc_h = 12; rtc_m = 34; rtc_s = 57;
	rtc_base_ms = now_ms;
	scr_clock.enter();
	frames(&scr_clock, 6);
	snapshot("2_clock_rtc");
	scr_clock.key(KIT_BTN_2);					/* nothing selected: no change */
	CHECK_EQ(rtc_h, 12);
	scr_clock.key(KIT_BTN_1);					/* hours */
	scr_clock.key(KIT_BTN_2);
	CHECK_EQ(rtc_h, 13);
	CHECK_EQ(rtc_s, 0);
	scr_clock.key(KIT_BTN_1);					/* minutes */
	for (int i = 0; i < 26; i++) {
		scr_clock.key(KIT_BTN_2);				/* 34 + 26 wraps to 0, hours untouched */
	}
	CHECK_EQ(rtc_m, 0);
	CHECK_EQ(rtc_h, 13);
	frames(&scr_clock, 1);
	snapshot("3_clock_set_minutes");
	scr_clock.key(KIT_BTN_1);					/* done */

	/* without an RTC: soft clock runs from the tick */
	rtc_present = 0;
	scr_clock.enter();
	scr_clock.key(KIT_BTN_1);
	scr_clock.key(KIT_BTN_2);					/* 14:00:00 on the soft clock */
	scr_clock.key(KIT_BTN_1);
	scr_clock.key(KIT_BTN_1);
	frames(&scr_clock, 20 * 61);				/* 61 seconds later */
	snapshot("4_clock_soft");
	CHECK_EQ(gfx_get(10, 14), 0);				/* "1" of 14: top segment off ... */
	CHECK_EQ(gfx_get(20, 20), 1);				/* ... right segment on */
}

static void test_snake(void) {
	int before;

	scr_snake.enter();
	frames(&scr_snake, 3);
	snapshot("5_snake");
	/* a square: right turns keep it away from the walls */
	before = beeps;
	for (int i = 0; i < 12; i++) {
		frames(&scr_snake, 6 * 3);
		scr_snake.key(KIT_BTN_2);
	}
	frames(&scr_snake, 1);
	scr_snake.key(KIT_BTN_3);					/* pause: the snake stays put */
	frames(&scr_snake, 200);
	snapshot("6_snake_pause");
	scr_snake.key(KIT_BTN_3);
	/* straight on into the wall */
	frames(&scr_snake, 6 * 40);
	snapshot("7_snake_game_over");
	CHECK(beeps > before);
	before = lit_pixels();
	frames(&scr_snake, 40);						/* dead: the picture no longer changes */
	CHECK_EQ(lit_pixels(), before);
	scr_snake.key(KIT_BTN_1);					/* new game */
	frames(&scr_snake, 2);
	CHECK(lit_pixels() != before);
}

static void test_flappy(void) {
	int before;

	scr_flappy.enter();
	frames(&scr_flappy, 2);
	snapshot("8_flappy_ready");
	before = lit_pixels();
	frames(&scr_flappy, 40);					/* waits for the first flap */
	CHECK_EQ(lit_pixels(), before);

	scr_flappy.key(KIT_BTN_1);
	for (int i = 0; i < 28; i++) {				/* a flap every 7 frames holds the height */
		frames(&scr_flappy, 1);
		if (i % 7 == 6) {
			scr_flappy.key(KIT_BTN_2);
		}
	}
	snapshot("9_flappy_flying");
	before = beeps;
	frames(&scr_flappy, 200);					/* no more flaps: down it goes */
	snapshot("10_flappy_game_over");
	CHECK(beeps > before);
	before = lit_pixels();
	frames(&scr_flappy, 20);
	CHECK_EQ(lit_pixels(), before);
}

static void test_system(void) {
	ui_last_pages = 3;
	ui_last_frame_ms = 9;
	scr_system.enter();
	frames(&scr_system, 2);
	snapshot("11_system");
	CHECK(lit_pixels() > 500);
}

int tt_checks;
int tt_fails;

int main(int argc, char** argv) {
	out_dir = (argc > 1) ? argv[1] : 0;
	host_reset_state(0xFF);
	host_use_virtual_time(1);
	task_init();

	if (argc > 2 && strcmp(argv[2], "record") == 0) {
		record();
		printf("recorded %u frames of virtual time\n", now_ms / UI_FRAME_MS);
		return 0;
	}

	printf("test_demo\n");
	RUN_TEST(test_gfx_flush_only_changes);
	RUN_TEST(test_text);
	RUN_TEST(test_menu);
	RUN_TEST(test_clock);
	RUN_TEST(test_snake);
	RUN_TEST(test_flappy);
	RUN_TEST(test_system);
	printf("%d checks, %d failed\n", tt_checks, tt_fails);
	return tt_fails ? 1 : 0;
}
