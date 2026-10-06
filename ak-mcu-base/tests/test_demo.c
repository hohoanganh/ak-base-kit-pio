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
#include "music.h"
#include "video.h"
#include "weather.h"
#include "crc.h"
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
static uint16_t buzzer_hz;
static uint8_t held_buttons;

uint8_t kit_buttons(void) { return held_buttons; }
void kit_buzzer(uint16_t freq_hz) { buzzer_hz = freq_hz; }

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

/* RS485 link of the Pong screen: what the kit sent, what it will receive */
static uint8_t link_open;
static uint8_t link_tx[4096];
static uint32_t link_tx_len;
static uint8_t link_rx[256];
static uint32_t link_rx_len, link_rx_pos;

void ui_link_open(void) { link_open = 1; link_rx_len = link_rx_pos = 0; link_tx_len = 0; }
void ui_link_close(void) { link_open = 0; }

int ui_link_getc(void) {
	return link_rx_pos < link_rx_len ? link_rx[link_rx_pos++] : -1;
}

void ui_link_write(const uint8_t* data, uint8_t len) {
	if (link_tx_len + len <= sizeof(link_tx)) {
		memcpy(link_tx + link_tx_len, data, len);
		link_tx_len += len;
	}
}

/* the other kit says something: one frame C5 type payload crc16 */
static void link_say(uint8_t type, const uint8_t* payload, uint8_t n) {
	uint8_t* f = link_rx;
	uint16_t crc;

	link_rx_pos = 0;
	f[0] = 0xC5;
	f[1] = type;
	memcpy(&f[2], payload, n);
	crc = crc16_update(CRC16_INIT, &f[1], (uint32_t)n + 1);
	f[2 + n] = (uint8_t)crc;
	f[3 + n] = (uint8_t)(crc >> 8);
	link_rx_len = (uint32_t)n + 4;
}

/* last frame of the given type the kit sent; 0 if none. Clears the log. */
static const uint8_t* link_heard(uint8_t type, uint8_t n) {
	static uint8_t found[12];
	const uint8_t* hit = 0;

	for (uint32_t i = 0; i + n + 4U <= link_tx_len; i++) {
		if (link_tx[i] == 0xC5 && link_tx[i + 1] == type) {
			uint16_t crc = crc16_update(CRC16_INIT, &link_tx[i + 1], (uint32_t)n + 1);

			if (link_tx[i + 2 + n] == (uint8_t)crc && link_tx[i + 3 + n] == (uint8_t)(crc >> 8)) {
				memcpy(found, &link_tx[i + 2], n);
				hit = found;
				i += n + 3U;
			}
		}
	}
	link_tx_len = 0;
	return hit;
}

/* SHT45: a slow wave around 26.5 C / 61 %, or no sensor at all */
static uint8_t sht_present = 1;
static uint8_t sht_started;

uint8_t kit_sht_start(void) {
	sht_started = sht_present;
	return sht_present;
}

uint8_t kit_sht_read(int16_t* t10, uint16_t* rh10) {
	if (!sht_present || !sht_started) {
		return 0;
	}
	sht_started = 0;
	*t10 = (int16_t)(265 + ui_sin((uint8_t)(now_ms / 700)) * 9 / 127 + ui_sin((uint8_t)(now_ms / 130)) / 64);
	*rh10 = (uint16_t)(610 - ui_sin((uint8_t)(now_ms / 900)) * 40 / 127);
	return 1;
}

/* media store: a piece of RAM that behaves like NOR flash (erased = 0xFF) */
static uint8_t store[48 * 1024];

uint32_t kit_store_size(void) { return sizeof(store); }

uint8_t kit_store_read(uint32_t off, void* buf, uint32_t len) {
	if (off > sizeof(store) || len > sizeof(store) - off) {
		return 0;
	}
	memcpy(buf, store + off, len);
	return 1;
}

uint8_t kit_store_erase(uint32_t off) {
	if (off % KIT_STORE_SECTOR || off >= sizeof(store)) {
		return 0;
	}
	memset(store + off, 0xFF, KIT_STORE_SECTOR);
	return 1;
}

uint8_t kit_store_write(uint32_t off, const void* data, uint32_t len) {
	if (off > sizeof(store) || len > sizeof(store) - off) {
		return 0;
	}
	for (uint32_t i = 0; i < len; i++) {
		store[off + i] &= ((const uint8_t*)data)[i];
	}
	return 1;
}

void ui_beep(uint16_t freq_hz, uint16_t ms) {
	(void)freq_hz;
	(void)ms;
	beeps++;
}

/* The firmware advances the song with a kernel timer; here the frames do it:
 * music_due_ms is when the piece that is sounding ends. */
static uint32_t music_due_ms;

void ui_music_play(const char* rtttl) {
	if (music_start(rtttl)) {
		music_due_ms = now_ms + music_step();
	}
}

void ui_music_stop(void) {
	music_stop();
}

static void music_run(void) {
	while (music_playing() && (int32_t)(now_ms - music_due_ms) >= 0) {
		uint16_t ms = music_step();

		if (!ms) {
			break;
		}
		music_due_ms += ms;
	}
}

/*----------------------------------------------------------------------------
 * helpers
 *--------------------------------------------------------------------------*/
static const char* out_dir;

static void frames(const ui_screen_t* s, int n) {
	while (n--) {
		now_ms += UI_FRAME_MS;
		host_advance_ms(UI_FRAME_MS);
		music_run();
		weather_poll(now_ms);
		if (link_open) {
			pong_poll();
		}
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
 * a small clip for the video player, drawn with gfx and packed like
 * tools/ak_video.py does it
 *--------------------------------------------------------------------------*/
#define CLIP_FRAMES		(60)

static uint8_t clip_src[CLIP_FRAMES][KIT_LCD_PAGES][KIT_LCD_W];

static uint32_t packbits(uint8_t* out, const uint8_t* in, int n) {
	uint32_t o = 0;
	int i = 0;

	while (i < n) {
		int run = 1;

		while (i + run < n && in[i + run] == in[i] && run < 129) {
			run++;
		}
		if (run >= 2) {
			out[o++] = (uint8_t)(run + 126);
			out[o++] = in[i];
			i += run;
		}
		else {
			int lit = 1;

			while (i + lit < n && lit < 128 && !(i + lit + 1 < n && in[i + lit] == in[i + lit + 1])) {
				lit++;
			}
			out[o++] = (uint8_t)(lit - 1);
			memcpy(out + o, in + i, (size_t)lit);
			o += (uint32_t)lit;
			i += lit;
		}
	}
	return o;
}

static void clip_draw(int f) {
	int bx = 12 + (f * 3) % 104, by = 40 + ui_sin((uint8_t)(f * 9)) * 8 / 127;

	gfx_clear();
	gfx_text_center(3, "AK VIDEO", 2);
	gfx_rect(0, 22, GFX_W, 42, 1);
	gfx_tri(bx - 8, by + 7, bx + 8, by + 7, bx, by - 8, (uint8_t)(4 + (f % 13)));
	gfx_fill(2, 58, 2 + f * 2, 4, 1);
}

/* Writes the clip into the store. Returns its size in bytes. */
static uint32_t clip_make(void) {
	uint8_t* p = store + VIDEO_HEADER_SIZE;
	uint8_t x[KIT_LCD_W];
	uint32_t len;

	for (uint32_t off = 0; off < sizeof(store); off += KIT_STORE_SECTOR) {
		kit_store_erase(off);
	}
	for (int f = 0; f < CLIP_FRAMES; f++) {
		uint8_t* masks = p;

		clip_draw(f);
		memcpy(clip_src[f], gfx_page(0), sizeof(clip_src[f]));
		p += 2;
		masks[0] = masks[1] = 0;
		for (int page = 0; page < KIT_LCD_PAGES; page++) {
			if (f == 0) {
				masks[0] |= (uint8_t)(1 << page);
				p += packbits(p, clip_src[f][page], KIT_LCD_W);
			}
			else if (memcmp(clip_src[f][page], clip_src[f - 1][page], KIT_LCD_W) != 0) {
				masks[0] |= (uint8_t)(1 << page);
				if (page & 1) {					/* odd pages as differences, to test both kinds */
					masks[1] |= (uint8_t)(1 << page);
					for (int i = 0; i < KIT_LCD_W; i++) {
						x[i] = clip_src[f][page][i] ^ clip_src[f - 1][page][i];
					}
					p += packbits(p, x, KIT_LCD_W);
				}
				else {
					p += packbits(p, clip_src[f][page], KIT_LCD_W);
				}
			}
		}
	}
	len = (uint32_t)(p - store) - VIDEO_HEADER_SIZE;
	memcpy(store, "AKV1", 4);
	store[4] = KIT_LCD_W; store[5] = KIT_LCD_H; store[6] = 20; store[7] = 0;
	store[8] = CLIP_FRAMES; store[9] = 0; store[10] = 0; store[11] = 0;
	store[12] = (uint8_t)len; store[13] = (uint8_t)(len >> 8); store[14] = (uint8_t)(len >> 16); store[15] = 0;
	gfx_clear();
	return len + VIDEO_HEADER_SIZE;
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
	clip_make();
	frames(0, 20 * 100);					/* the weather graph has something to show */

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
	play(&scr_snake, 160);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_flappy.enter();
	play(&scr_flappy, 140);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_dino.enter();
	play(&scr_dino, 140);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_tetris.enter();
	play(&scr_tetris, 200);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_breakout.enter();
	play(&scr_breakout, 160);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_invaders.enter();
	play(&scr_invaders, 160);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_pong.enter();
	play(&scr_pong, 140);
	scr_pong.leave();
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_cube.enter();
	play(&scr_cube, 60);
	scr_cube.key(KIT_BTN_1);
	play(&scr_cube, 50);
	scr_cube.key(KIT_BTN_1);
	play(&scr_cube, 50);
	scr_cube.key(KIT_BTN_1);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_maze.enter();
	play(&scr_maze, 160);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_music.enter();
	play(&scr_music, 100);
	scr_music.leave();
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_video.enter();
	play(&scr_video, CLIP_FRAMES);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_weather.enter();
	play(&scr_weather, 60);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_saver.enter();
	play(&scr_saver, 70);
	scr_saver.key(KIT_BTN_1);
	play(&scr_saver, 60);
	scr_saver.key(KIT_BTN_1);
	play(&scr_saver, 60);
	scr_saver.key(KIT_BTN_1);
	play(0, 8);
	menu_press(KIT_BTN_1, 8);
	scr_system.enter();
	play(&scr_system, 50);
	play(0, 6);
	menu_press(KIT_BTN_1, 10);				/* wraps to the first entry: the GIF loops cleanly */
	rec_close();

	rec_open("pong");
	scr_pong.enter();
	play(&scr_pong, 500);
	scr_pong.leave();
	rec_close();

	rec_open("tetris");
	scr_tetris.enter();
	play(&scr_tetris, 600);
	rec_close();

	rec_open("breakout");
	scr_breakout.enter();
	play(&scr_breakout, 500);
	rec_close();

	rec_open("invaders");
	scr_invaders.enter();
	play(&scr_invaders, 500);
	rec_close();

	ui_autoplay = 0;
	rec_open("saver");
	scr_saver.enter();
	play(&scr_saver, 200);
	scr_saver.key(KIT_BTN_1);
	play(&scr_saver, 120);
	scr_saver.key(KIT_BTN_1);
	play(&scr_saver, 120);
	scr_saver.key(KIT_BTN_1);
	rec_close();
	ui_autoplay = 1;

	rec_open("maze");
	scr_maze.enter();
	play(&scr_maze, 600);
	rec_close();

	rec_open("weather");
	scr_weather.enter();
	play(&scr_weather, 150);
	scr_weather.key(KIT_BTN_1);
	play(&scr_weather, 100);
	scr_weather.key(KIT_BTN_1);
	rec_close();

	rec_open("video");
	scr_video.enter();
	play(&scr_video, 2 * CLIP_FRAMES);
	rec_close();

	rec_open("dino");
	scr_dino.enter();
	play(&scr_dino, 520);
	rec_close();

	rec_open("3d");
	scr_cube.enter();
	play(&scr_cube, 128);					/* cube: 128 frames = one full turn of the slow axis */
	scr_cube.key(KIT_BTN_1);
	play(&scr_cube, 128);
	scr_cube.key(KIT_BTN_1);
	play(&scr_cube, 128);
	scr_cube.key(KIT_BTN_3);				/* the same pyramid as a wireframe */
	play(&scr_cube, 64);
	scr_cube.key(KIT_BTN_3);
	scr_cube.key(KIT_BTN_1);
	rec_close();

	rec_open("music");
	scr_music.enter();
	play(&scr_music, 400);
	scr_music.leave();
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

static void test_gfx_shapes(void) {
	gfx_clear();
	gfx_line(0, 0, 9, 9, 1);					/* diagonal: exactly its 10 pixels */
	gfx_flush(0);
	CHECK_EQ(lit_pixels(), 10);
	CHECK_EQ(gfx_get(5, 5), 1);
	gfx_clear();
	gfx_line(20, 30, 20, 30, 1);				/* a single point */
	gfx_line(0, 63, 127, 63, 1);
	gfx_line(127, 0, 127, 63, 1);
	gfx_flush(0);
	CHECK_EQ(lit_pixels(), 1 + 128 + 63);
	gfx_clear();
	gfx_tri(10, 10, 50, 10, 10, 50, 16);		/* white: about half of a 41 x 41 square */
	gfx_flush(0);
	CHECK(lit_pixels() > 780 && lit_pixels() < 900);
	gfx_tri(10, 10, 50, 10, 10, 50, 8);			/* 50 % grey: half of that, and it clears the rest */
	gfx_flush(0);
	CHECK(lit_pixels() > 380 && lit_pixels() < 460);
	gfx_tri(10, 10, 50, 10, 10, 50, 0);
	gfx_flush(0);
	CHECK_EQ(lit_pixels(), 0);
	gfx_tri(-40, -40, 300, 20, 60, 200, 16);	/* far outside the screen: clipped, no crash */
}

static void test_cube(void) {
	int a, b;

	scr_cube.enter();
	frames(&scr_cube, 9);
	snapshot("12_3d_cube");
	a = lit_pixels();
	CHECK(a > 250);
	frames(&scr_cube, 7);
	CHECK(lit_pixels() != a);					/* it turns */
	scr_cube.key(KIT_BTN_3);					/* wireframe: far fewer pixels */
	frames(&scr_cube, 1);
	snapshot("13_3d_wire");
	b = lit_pixels();
	CHECK(b < a);
	scr_cube.key(KIT_BTN_3);
	scr_cube.key(KIT_BTN_1);
	frames(&scr_cube, 13);
	snapshot("14_3d_octa");
	scr_cube.key(KIT_BTN_1);
	frames(&scr_cube, 21);
	snapshot("15_3d_pyramid");
	scr_cube.key(KIT_BTN_2);					/* fast */
	scr_cube.key(KIT_BTN_2);					/* stopped: the picture stands still */
	frames(&scr_cube, 1);
	a = lit_pixels();
	frames(&scr_cube, 10);
	CHECK_EQ(lit_pixels(), a);
	scr_cube.key(KIT_BTN_2);
	scr_cube.key(KIT_BTN_1);					/* back to the cube for the next run */
	/* a full turn on both axes never draws outside the frame buffer (ASan) */
	frames(&scr_cube, 300);
}

static void test_dino(void) {
	int before;

	ui_autoplay = 0;
	scr_dino.enter();
	frames(&scr_dino, 2);
	snapshot("16_dino_ready");
	before = lit_pixels();
	frames(&scr_dino, 30);						/* waits for the first key */
	CHECK_EQ(lit_pixels(), before);

	/* never jumping: runs into the first cactus */
	scr_dino.key(KIT_BTN_2);					/* a duck starts the game too */
	before = beeps;
	frames(&scr_dino, 200);
	snapshot("17_dino_game_over");
	CHECK(beeps > before);
	before = lit_pixels();
	frames(&scr_dino, 30);
	CHECK_EQ(lit_pixels(), before);

	/* the autopilot jumps and ducks its way through a long run */
	ui_autoplay = 1;
	scr_dino.key(KIT_BTN_1);
	frames(&scr_dino, 100);
	snapshot("18_dino_running");
	before = beeps;
	frames(&scr_dino, 900);
	snapshot("19_dino_later");
	CHECK(beeps - before > 20);					/* jumps + the beep every 100 points */
	ui_autoplay = 0;
}

static void test_music(void) {
	uint32_t ms;
	char name[16];

	/* d=4, b=120: a quarter is 500 ms. Dots, sharps, octaves, a pause. */
	static const char* song = "Test:d=4,o=5,b=120:a4,8c,c#6,2p,e.,8g#7.";

	CHECK_EQ(music_measure(song, &ms), 6);
	CHECK_EQ(ms, 500 + 250 + 500 + 1000 + 750 + 375);
	CHECK(strcmp(music_name(song, name, sizeof(name)), "Test") == 0);
	CHECK_EQ(music_measure("no header at all", &ms), 0);
	CHECK_EQ(music_start("broken:d=4"), 0);
	CHECK_EQ(music_playing(), 0);

	CHECK_EQ(music_start(song), 1);
	CHECK_EQ(music_step(), 438);				/* A4 for 7/8 of 500 ms ... */
	CHECK_EQ(buzzer_hz, 440);
	CHECK_EQ(music_note(), 10);
	CHECK_EQ(music_octave(), 4);
	CHECK_EQ(music_step(), 62);					/* ... then the gap */
	CHECK_EQ(buzzer_hz, 0);
	CHECK_EQ(music_step(), 219);				/* C5, an eighth */
	CHECK_EQ(buzzer_hz, 523);
	music_step();
	music_step();								/* C#6 */
	CHECK_EQ(buzzer_hz, 1108);
	music_step();
	CHECK_EQ(music_step(), 1000);				/* the pause: silent, no gap after it */
	CHECK_EQ(buzzer_hz, 0);
	CHECK_EQ(music_note(), 0);
	music_step();								/* E5 dotted */
	CHECK_EQ(buzzer_hz, 659);
	music_step();
	music_step();								/* G#7 dotted eighth */
	CHECK_EQ(buzzer_hz, 3322);
	CHECK_EQ(music_count(), 6);
	music_step();
	CHECK_EQ(music_step(), 0);					/* end of the song */
	CHECK_EQ(music_playing(), 0);
	CHECK_EQ(buzzer_hz, 0);

	/* the jukebox: play, the bars fill up, next song keeps playing, stop */
	scr_music.enter();
	frames(&scr_music, 2);
	snapshot("20_music_stopped");
	scr_music.key(KIT_BTN_2);
	CHECK_EQ(music_playing(), 1);
	frames(&scr_music, 20 * 12);
	snapshot("21_music_playing");
	CHECK(music_count() > 20);
	scr_music.key(KIT_BTN_1);
	CHECK_EQ(music_playing(), 1);
	frames(&scr_music, 20 * 5);
	snapshot("22_music_next_song");
	scr_music.key(KIT_BTN_2);
	CHECK_EQ(music_playing(), 0);
	CHECK_EQ(buzzer_hz, 0);
	scr_music.key(KIT_BTN_2);
	scr_music.leave();							/* leaving the screen silences the buzzer */
	CHECK_EQ(music_playing(), 0);
	/* every built-in song parses to the end and has a sane length */
	for (int i = 0; i < 6; i++) {
		scr_music.key(KIT_BTN_2);
		CHECK_EQ(music_playing(), 1);
		frames(&scr_music, 20 * 60);
		CHECK_EQ(music_playing(), 0);			/* over within a minute */
		scr_music.key(KIT_BTN_1);
	}
}

static void test_video(void) {
	uint32_t size;
	uint8_t saved;

	/* empty flash: the screen says so */
	memset(store, 0xFF, sizeof(store));
	scr_video.enter();
	frames(&scr_video, 2);
	snapshot("23_video_no_clip");
	CHECK(lit_pixels() > 300);
	CHECK_EQ(video_open(kit_store_read, kit_store_size()), 0);

	size = clip_make();
	CHECK(size < CLIP_FRAMES * 300U);			/* far below 1 KB per frame */
	CHECK_EQ(video_open(kit_store_read, kit_store_size()), 1);
	CHECK_EQ(video_frames(), CLIP_FRAMES);
	CHECK_EQ(video_fps(), 20);
	CHECK_EQ(video_size(), size);

	/* 20 frames per second = one per UI frame: every picture must match its source */
	scr_video.enter();
	for (int f = 0; f < CLIP_FRAMES; f++) {
		frames(&scr_video, 1);
		if (memcmp(panel, clip_src[f], sizeof(panel)) != 0) {
			CHECK_EQ(f, -1);
			break;
		}
		if (f == 25) {
			snapshot("24_video_playing");
		}
	}
	CHECK_EQ(video_pos(), CLIP_FRAMES);
	frames(&scr_video, 3);						/* loops */
	CHECK(memcmp(panel, clip_src[2], sizeof(panel)) == 0);

	scr_video.key(KIT_BTN_1);					/* pause: the picture stays */
	frames(&scr_video, 20);
	CHECK(memcmp(panel, clip_src[2], sizeof(panel)) == 0);
	scr_video.key(KIT_BTN_1);					/* play: goes on where it stopped */
	frames(&scr_video, 1);
	CHECK(memcmp(panel, clip_src[3], sizeof(panel)) == 0);
	scr_video.key(KIT_BTN_2);					/* back to the start */
	frames(&scr_video, 1);
	CHECK(memcmp(panel, clip_src[0], sizeof(panel)) == 0);

	/* a clip longer than the store says it is, or with damaged data, never
	 * writes outside the frame buffer (ASan watches) and ends cleanly */
	saved = store[13];
	store[13] = 0xFF;
	CHECK_EQ(video_open(kit_store_read, kit_store_size()), 0);
	store[13] = saved;
	for (uint32_t i = VIDEO_HEADER_SIZE; i < size; i += 7) {
		store[i] ^= 0x5A;
	}
	CHECK_EQ(video_open(kit_store_read, kit_store_size()), 1);
	{
		int n = 0;

		while (video_next(gfx_fb())) {
			n++;
		}
		CHECK(n < CLIP_FRAMES);
	}
	scr_video.enter();
	frames(&scr_video, 200);
	clip_make();								/* leave a good clip for later */
	gfx_clear();
}

static void test_weather(void) {
	int16_t t;
	uint16_t h;

	sht_present = 1;
	scr_weather.enter();
	for (int i = 0; i < 3; i++) {
		scr_weather.key(KIT_BTN_2);				/* once round the rates: an empty graph */
	}
	frames(&scr_weather, 30);					/* 1.5 s: at least one reading */
	CHECK_EQ(weather_now(&t, &h), 1);
	CHECK(t > 240 && t < 290);
	CHECK(h > 550 && h < 670);
	CHECK(weather_count() >= 1);
	frames(&scr_weather, 20 * 60);				/* a minute at one point per second */
	CHECK(weather_count() >= 59 && weather_count() <= 63);
	snapshot("25_weather_temperature");
	scr_weather.key(KIT_BTN_1);
	frames(&scr_weather, 20 * 60);				/* the graph is full and scrolls */
	CHECK_EQ(weather_count(), WEATHER_HIST);
	snapshot("26_weather_humidity");
	scr_weather.key(KIT_BTN_1);

	scr_weather.key(KIT_BTN_2);					/* one point per minute: a new graph */
	CHECK_EQ(weather_period_s(), 60);
	CHECK_EQ(weather_count(), 0);
	frames(&scr_weather, 20 * 150);
	CHECK_EQ(weather_count(), 3);
	scr_weather.key(KIT_BTN_2);
	scr_weather.key(KIT_BTN_2);					/* back to seconds */
	CHECK_EQ(weather_period_s(), 1);

	{
		char buf[8];

		CHECK(strcmp(weather_fmt(buf, 234), "23.4") == 0);
		CHECK(strcmp(weather_fmt(buf, -5), "-0.5") == 0);
		CHECK(strcmp(weather_fmt(buf, 1000), "100.0") == 0);
	}

	sht_present = 0;							/* sensor gone */
	frames(&scr_weather, 30);
	CHECK_EQ(weather_now(&t, &h), 0);
	snapshot("27_weather_no_sensor");
	sht_present = 1;
	frames(&scr_weather, 30);
	CHECK_EQ(weather_now(&t, &h), 1);
}

static void test_maze(void) {
	int a;

	ui_autoplay = 0;
	scr_maze.enter();
	frames(&scr_maze, 2);
	snapshot("28_maze_start");
	a = lit_pixels();
	CHECK(a > 600);
	frames(&scr_maze, 10);
	CHECK_EQ(lit_pixels(), a);					/* standing still: nothing changes */
	scr_maze.key(KIT_BTN_2);					/* a tap turns 1/16 of a circle */
	frames(&scr_maze, 6);
	snapshot("29_maze_turned");
	CHECK(lit_pixels() != a);
	scr_maze.key(KIT_BTN_1);
	frames(&scr_maze, 6);
	CHECK_EQ(lit_pixels(), a);					/* and back: the same picture */
	scr_maze.key(KIT_BTN_3);					/* walk into the wall ahead: stops there, no crash */
	frames(&scr_maze, 200);
	snapshot("30_maze_at_wall");
	held_buttons = KIT_BTN_2;					/* held: keeps turning, sliding along walls */
	frames(&scr_maze, 300);
	held_buttons = 0;

	/* the maze walks itself for five minutes: every view it can produce stays
	 * inside the frame buffer (ASan) */
	ui_autoplay = 1;
	scr_maze.enter();
	frames(&scr_maze, 120);
	snapshot("31_maze_walking");
	frames(&scr_maze, 20 * 300);
	snapshot("32_maze_later");
	ui_autoplay = 0;
}

static void test_tetris(void) {
	int a;

	ui_autoplay = 0;
	scr_tetris.enter();
	frames(&scr_tetris, 2);
	a = lit_pixels();
	frames(&scr_tetris, 30);
	CHECK_EQ(lit_pixels(), a);					/* waits for the first key */
	scr_tetris.key(KIT_BTN_3);
	frames(&scr_tetris, 12);
	CHECK(lit_pixels() != a);					/* the piece falls */
	snapshot("33_tetris_start");
	/* hammer the keys: left wall, right wall, turning against both, dropping */
	for (int i = 0; i < 400; i++) {
		scr_tetris.key((i / 20) & 1 ? KIT_BTN_1 : KIT_BTN_2);
		if (i % 3 == 0) {
			scr_tetris.key(KIT_BTN_3);
		}
		held_buttons = (i % 50 > 40) ? (KIT_BTN_1 | KIT_BTN_2) : (i % 7 == 0 ? KIT_BTN_1 : 0);
		frames(&scr_tetris, 1);
	}
	held_buttons = 0;
	/* without steering the pile reaches the top: game over, and a key starts again */
	scr_tetris.enter();
	scr_tetris.key(KIT_BTN_1);
	frames(&scr_tetris, 20 * 150);
	snapshot("34_tetris_game_over");
	a = lit_pixels();
	frames(&scr_tetris, 20);
	CHECK_EQ(lit_pixels(), a);					/* nothing moves any more */
	scr_tetris.key(KIT_BTN_1);
	frames(&scr_tetris, 1);
	CHECK(lit_pixels() < a);					/* empty well */

	ui_autoplay = 1;							/* plays itself for five minutes */
	scr_tetris.enter();
	frames(&scr_tetris, 20 * 40);
	snapshot("35_tetris_autoplay");
	frames(&scr_tetris, 20 * 260);
	ui_autoplay = 0;
}

static void test_breakout(void) {
	int a, full;

	ui_autoplay = 0;
	scr_breakout.enter();
	frames(&scr_breakout, 2);
	snapshot("36_breakout_serve");
	a = full = lit_pixels();
	scr_breakout.key(KIT_BTN_2);				/* the paddle moves, the ball rides on it */
	frames(&scr_breakout, 3);
	CHECK_EQ(lit_pixels(), a);
	scr_breakout.key(KIT_BTN_3);				/* serve; nobody plays: three balls are lost */
	for (int i = 0; i < 2; i++) {
		frames(&scr_breakout, 20 * 30);
		scr_breakout.key(KIT_BTN_3);
	}
	frames(&scr_breakout, 20 * 30);
	snapshot("37_breakout_game_over");
	held_buttons = KIT_BTN_1;					/* held against the left edge, then the right one */
	scr_breakout.key(KIT_BTN_3);
	scr_breakout.key(KIT_BTN_3);
	frames(&scr_breakout, 100);
	held_buttons = KIT_BTN_2;
	frames(&scr_breakout, 100);
	held_buttons = 0;

	ui_autoplay = 1;							/* clears several walls */
	scr_breakout.enter();
	frames(&scr_breakout, 20 * 25);
	snapshot("38_breakout_autoplay");
	CHECK(lit_pixels() < full - 200);			/* bricks are gone */
	frames(&scr_breakout, 20 * 400);
	ui_autoplay = 0;
}

static void test_invaders(void) {
	int a;

	ui_autoplay = 0;
	scr_invaders.enter();
	frames(&scr_invaders, 2);
	snapshot("39_invaders_ready");
	a = lit_pixels();
	frames(&scr_invaders, 30);
	CHECK_EQ(lit_pixels(), a);					/* waits for a key */
	scr_invaders.key(KIT_BTN_3);				/* nobody plays on: bombs or the fleet end the game */
	frames(&scr_invaders, 20 * 120);
	snapshot("40_invaders_game_over");
	held_buttons = KIT_BTN_1;
	scr_invaders.key(KIT_BTN_3);
	scr_invaders.key(KIT_BTN_3);
	for (int i = 0; i < 200; i++) {
		scr_invaders.key(KIT_BTN_3);
		frames(&scr_invaders, 1);
		if (i == 100) {
			held_buttons = KIT_BTN_2;
		}
	}
	held_buttons = 0;

	ui_autoplay = 1;
	scr_invaders.enter();
	frames(&scr_invaders, 20 * 12);
	snapshot("41_invaders_autoplay");
	frames(&scr_invaders, 20 * 400);
	ui_autoplay = 0;
}

static void test_saver(void) {
	int a, b;

	ui_autoplay = 0;
	scr_saver.enter();							/* Game of Life */
	frames(&scr_saver, 40);
	snapshot("42_saver_life");
	a = lit_pixels();
	CHECK(a > 200 && a < 4000);
	CHECK(a % 4 == 0);							/* cells are 2x2 pixels */
	frames(&scr_saver, 4);
	CHECK(lit_pixels() != a);					/* it lives */
	frames(&scr_saver, 20 * 200);				/* long enough to die out or freeze: it starts again */
	CHECK(lit_pixels() > 40);

	scr_saver.key(KIT_BTN_1);					/* star field */
	frames(&scr_saver, 60);
	snapshot("43_saver_stars");
	b = lit_pixels();
	CHECK(b >= 20 && b < 200);
	for (int i = 0; i < 6; i++) {				/* every speed */
		scr_saver.key(KIT_BTN_2);
		frames(&scr_saver, 300);
	}

	scr_saver.key(KIT_BTN_1);					/* plasma: about half the screen lit */
	frames(&scr_saver, 40);
	snapshot("44_saver_plasma");
	a = lit_pixels();
	CHECK(a > 2500 && a < 5700);
	for (int i = 0; i < 6; i++) {
		scr_saver.key(KIT_BTN_2);
		frames(&scr_saver, 300);
	}
	scr_saver.key(KIT_BTN_1);					/* back to Life */
}

/* rows of the screen column x that are lit, as first row (or -1) */
static int column_top(int x) {
	for (int y = 10; y < KIT_LCD_H; y++) {
		if ((panel[y >> 3][x] >> (y & 7)) & 1) {
			return y;
		}
	}
	return -1;
}

static void test_pong(void) {
	const uint8_t* f;
	uint8_t p[8];
	uint16_t my_id;
	int a;

	ui_autoplay = 1;
	scr_pong.enter();
	CHECK_EQ(link_open, 1);
	frames(&scr_pong, 40);
	snapshot("45_pong_alone");
	/* alone: the kit calls hello, and plays against itself */
	f = link_heard('H', 2);
	CHECK(f != 0);
	my_id = (uint16_t)(f[0] | (f[1] << 8));
	CHECK(my_id != 0);
	CHECK(link_heard('S', 7) == 0);
	frames(&scr_pong, 20 * 120);				/* two minutes: rallies, points, no crash */
	link_heard('H', 2);

	/* a kit with a lower number answers: this one becomes the host and sends the game */
	p[0] = (uint8_t)(my_id - 1); p[1] = (uint8_t)((my_id - 1) >> 8);
	link_say('H', p, 2);
	frames(&scr_pong, 2);
	f = link_heard('S', 7);
	CHECK(f != 0);
	if (f) {
		CHECK(f[1] < KIT_LCD_W && f[2] >= 10 && f[2] < KIT_LCD_H);
		CHECK_EQ(f[4], 0);						/* a new game */
		CHECK_EQ(f[5], 0);
	}
	snapshot("46_pong_host");
	/* the guest moves its paddle: it shows on the right edge */
	p[0] = 1; p[1] = 10;
	link_say('P', p, 2);
	frames(&scr_pong, 1);
	CHECK_EQ(column_top(124), 10);
	p[0] = 2; p[1] = 52;
	link_say('P', p, 2);
	frames(&scr_pong, 1);
	CHECK_EQ(column_top(124), 52);
	p[0] = 3; p[1] = 200;						/* nonsense position: ignored */
	link_say('P', p, 2);
	frames(&scr_pong, 1);
	CHECK_EQ(column_top(124), 52);
	/* the guest falls silent: after a second the kit is alone again */
	frames(&scr_pong, 30);
	link_heard('S', 7);
	frames(&scr_pong, 20);
	CHECK(link_heard('S', 7) == 0);


	/* a host appears: this kit becomes the guest, shows the game mirrored and answers */
	p[0] = 7; p[1] = 20; p[2] = 30; p[3] = 40; p[4] = 3; p[5] = 5; p[6] = 0;
	link_say('S', p, 7);
	frames(&scr_pong, 1);
	snapshot("47_pong_guest");
	f = link_heard('P', 2);
	CHECK(f != 0);
	if (f) {
		CHECK_EQ(f[0], 7);						/* same sequence number */
		CHECK(f[1] >= 10 && f[1] <= KIT_LCD_H - 12);
	}
	CHECK_EQ(column_top(124), 40);				/* the host's paddle is on the right here */
	CHECK_EQ(column_top(KIT_LCD_W - 2 - 20), 30);	/* ball at x 20 of the host = mirrored */
	a = lit_pixels();
	/* damaged frame and noise: ignored, nothing is sent */
	link_say('S', p, 7);
	link_rx[5] ^= 0x40;
	frames(&scr_pong, 1);
	CHECK(link_heard('P', 2) == 0);
	CHECK_EQ(lit_pixels(), a);
	for (uint32_t i = 0; i < sizeof(link_rx); i++) {
		link_rx[i] = (uint8_t)(i * 37 + 0xC5 * (i % 5 == 0));
	}
	link_rx_pos = 0;
	link_rx_len = sizeof(link_rx);
	frames(&scr_pong, 3);
	link_rx_len = 0;

	scr_pong.leave();
	CHECK_EQ(link_open, 0);						/* the port goes back to Modbus */
	ui_autoplay = 0;
}

/* the screen mirror: every page, sent as text, decodes to what is on screen */
static char dump_text[600];
static uint32_t dump_len;

static void dump_put(uint8_t c) {
	if (dump_len < sizeof(dump_text) - 1) {
		dump_text[dump_len++] = (char)c;
	}
}

static int hex_val(char c) {
	return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static void test_screen_dump(void) {
	static const ui_screen_t* const screens[] = { 0, &scr_clock, &scr_saver, &scr_system };

	for (uint32_t s = 0; s < sizeof(screens) / sizeof(screens[0]); s++) {
		if (screens[s]) {
			screens[s]->enter();
			if (screens[s] == &scr_saver) {
				scr_saver.key(KIT_BTN_1);
				scr_saver.key(KIT_BTN_1);		/* plasma: the hardest picture to pack */
			}
		}
		frames(screens[s], 45);
		for (uint8_t page = 0; page < KIT_LCD_PAGES; page++) {
			uint8_t raw[160], out[KIT_LCD_W + 130];
			uint32_t n = 0, o = 0, i = 0;
			uint16_t chars, crc;
			const char* h;

			dump_len = 0;
			chars = ui_dump_page(page, dump_put);
			dump_text[dump_len] = 0;
			CHECK_EQ(chars, dump_len);
			CHECK(dump_len <= 4 + 2 * 130 + 6);			/* never much longer than the raw page */
			CHECK(dump_text[0] == '@' && dump_text[1] == 'P' && dump_text[2] == '0' + page && dump_text[3] == ' ');
			CHECK_EQ(dump_text[dump_len - 1], '\n');
			for (h = dump_text + 4; hex_val(h[0]) >= 0 && hex_val(h[1]) >= 0 && n < sizeof(raw); h += 2) {
				raw[n++] = (uint8_t)(hex_val(h[0]) * 16 + hex_val(h[1]));
			}
			CHECK_EQ(*h, ' ');
			while (i < n && o < KIT_LCD_W) {				/* PackBits */
				uint8_t c = raw[i++];

				if (c < 128) {
					memcpy(out + o, raw + i, (size_t)c + 1);
					o += (uint32_t)c + 1;
					i += (uint32_t)c + 1;
				}
				else {
					memset(out + o, raw[i++], (size_t)c - 126);
					o += (uint32_t)c - 126;
				}
			}
			CHECK_EQ(o, KIT_LCD_W);
			CHECK_EQ(i, n);
			CHECK(memcmp(out, panel[page], KIT_LCD_W) == 0);
			crc = crc16_update(CRC16_INIT, panel[page], KIT_LCD_W);
			CHECK(hex_val(h[1]) * 16 + hex_val(h[2]) == (crc >> 8) && hex_val(h[3]) * 16 + hex_val(h[4]) == (crc & 0xFF));
		}
	}
	scr_saver.key(KIT_BTN_1);						/* back to Life for whoever comes next */
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
	RUN_TEST(test_gfx_shapes);
	RUN_TEST(test_cube);
	RUN_TEST(test_dino);
	RUN_TEST(test_music);
	RUN_TEST(test_video);
	RUN_TEST(test_weather);
	RUN_TEST(test_maze);
	RUN_TEST(test_tetris);
	RUN_TEST(test_breakout);
	RUN_TEST(test_invaders);
	RUN_TEST(test_saver);
	RUN_TEST(test_pong);
	RUN_TEST(test_screen_dump);
	printf("%d checks, %d failed\n", tt_checks, tt_fails);
	return tt_fails ? 1 : 0;
}
