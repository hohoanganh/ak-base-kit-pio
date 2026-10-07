/**
 * Scope: a scrolling graph of numbers sent to the kit, a small plotter for
 * the bench.
 *   B1: hold / run
 *   B2: clear
 * Where the numbers come from:
 *   - the shell:  plot <number>          (one point per line, -32768..32767)
 *   - Modbus:     write holding register 16 (as a signed 16-bit value)
 * The graph scales itself to the points on screen. With "ui auto" and nothing
 * arriving, the screen draws a test wave of its own.
 */
#include <string.h>

#include "ui.h"
#include "plot.h"

#define GRAPH_X			(4)
#define GRAPH_Y			(29)
#define GRAPH_H			(24)
#define QUIET_FRAMES	(40)		/* no sample for this long: the test wave may play */

static int16_t points[PLOT_POINTS];		/* oldest first */
static uint8_t count;
static uint8_t hold;
static uint16_t got;					/* samples since the last rate update */
static uint16_t rate;					/* samples per second */
static uint8_t rate_frames;
static uint8_t quiet;					/* frames since the last sample from outside */
static uint8_t own_wave;				/* the points on screen are the test wave */
static uint16_t wave_t;

static void push(int16_t v) {
	if (count == PLOT_POINTS) {
		memmove(points, points + 1, sizeof(points) - sizeof(points[0]));
		count--;
	}
	points[count++] = v;
}

void plot_add(int16_t v) {
	quiet = 0;
	got++;
	if (hold) {
		return;
	}
	if (own_wave) {						/* real data replaces the test wave */
		own_wave = 0;
		count = 0;
	}
	push(v);
}

uint8_t plot_count(void) {
	return count;
}

static void plot_enter(void) {
	rate_frames = 0;
	got = 0;
	rate = 0;
}

static void plot_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		hold = !hold;
	}
	else if (btn == KIT_BTN_2) {
		count = 0;
		own_wave = 0;
	}
	ui_beep(1800, 15);
}

/* "-123" into buf, returns buf */
static char* itoa16(char* buf, int v) {
	if (v < 0) {
		buf[0] = '-';
		gfx_utoa(buf + 1, (uint32_t)-v, 1);
	}
	else {
		gfx_utoa(buf, (uint32_t)v, 1);
	}
	return buf;
}

static void plot_frame(uint32_t now_ms) {
	char buf[12];
	int lo = 32767, hi = -32768;

	(void)now_ms;
	if (++rate_frames >= 1000 / UI_FRAME_MS) {
		rate_frames = 0;
		rate = got;
		got = 0;
	}
	if (quiet < 255) {
		quiet++;
	}
	if (ui_autoplay && quiet > QUIET_FRAMES && !hold) {
		if (!own_wave) {
			own_wave = 1;
			count = 0;
		}
		wave_t++;
		push((int16_t)(ui_sin((uint8_t)(wave_t * 5)) * 6 + ui_sin((uint8_t)(wave_t * 17)) * 2
					   + (int)(ui_rand() % 61) - 30));
	}

	gfx_clear();
	if (hold) {
		ui_title("SCOPE", "hold");
	}
	else if (own_wave) {
		ui_title("SCOPE", "test wave");
	}
	else {
		char r[16];

		gfx_utoa(r, rate, 1);
		strcat(r, "/s");
		ui_title("SCOPE", r);
	}

	if (count == 0) {
		gfx_text_center(14, "send numbers:", 1);
		gfx_text_center(26, "shell  plot 123", 1);
		gfx_text_center(36, "Modbus reg 16", 1);
		ui_footer("HOLD", "CLEAR", "hold:MENU");
		return;
	}

	for (uint8_t i = 0; i < count; i++) {
		if (points[i] < lo) { lo = points[i]; }
		if (points[i] > hi) { hi = points[i]; }
	}
	/* newest value in big digits, the range of the graph next to it */
	gfx_text(1, 11, itoa16(buf, points[count - 1]), 2);
	itoa16(buf, hi);
	gfx_text(GFX_W - gfx_text_width(buf, 1), 11, buf, 1);
	itoa16(buf, lo);
	gfx_text(GFX_W - gfx_text_width(buf, 1), 20, buf, 1);
	if (hi - lo < 8) {						/* a flat line sits in the middle, not on an edge */
		lo -= (8 - (hi - lo)) / 2;
		hi = lo + 8;
	}

	if (lo < 0 && hi > 0) {					/* where zero is */
		int y0 = GRAPH_Y + GRAPH_H - 1 - (int)((int32_t)(0 - lo) * (GRAPH_H - 1) / (hi - lo));

		for (int x = GRAPH_X; x < GRAPH_X + PLOT_POINTS; x += 4) {
			gfx_pixel(x, y0, 1);
		}
	}
	for (uint8_t i = 0; i < count; i++) {
		int x = GRAPH_X + (PLOT_POINTS - count) + i;		/* newest at the right edge */
		int y = GRAPH_Y + GRAPH_H - 1 - (int)((int32_t)(points[i] - lo) * (GRAPH_H - 1) / (hi - lo));

		if (i > 0) {
			int py = GRAPH_Y + GRAPH_H - 1 - (int)((int32_t)(points[i - 1] - lo) * (GRAPH_H - 1) / (hi - lo));

			gfx_line(x - 1, py, x, y, 1);
		}
		else {
			gfx_pixel(x, y, 1);
		}
	}
	ui_footer(hold ? "RUN" : "HOLD", "CLEAR", "hold:MENU");
}

const ui_screen_t scr_plot = { "Scope", plot_enter, plot_key, plot_frame, 0 };
