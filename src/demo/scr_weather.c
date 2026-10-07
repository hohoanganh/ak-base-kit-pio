/**
 * Weather station: temperature and humidity from the SHT45 of the kit, with a
 * graph of the last 96 samples.
 *   B1: graph shows temperature / humidity
 *   B2: time between two points of the graph: 1 s, 1 min, 15 min (= 24 hours
 *       across the screen). Changing it starts a new graph.
 * The sensor is read once a second whatever screen is open (weather_poll),
 * so the graph keeps filling while a game runs. Shell: "th" prints the
 * values and the graph as CSV.
 */
#include <string.h>

#include "ui.h"
#include "weather.h"

#define READ_MS			(1000)
#define GRAPH_X			(31)
#define GRAPH_Y			(29)
#define GRAPH_H			(23)

static const uint16_t periods_s[] = { 1, 60, 900 };
static const char* const period_names[] = { "1 s", "1 min", "15 min" };

static int16_t hist_t[WEATHER_HIST];		/* 0.1 C, oldest first */
static uint16_t hist_h[WEATHER_HIST];		/* 0.1 % */
static uint8_t hist_n;
static int16_t cur_t;
static uint16_t cur_h;
static uint8_t have_value;					/* a reading arrived */
static uint8_t pending;						/* measurement started, result not fetched */
static uint8_t started;
static uint8_t period_idx;
static uint8_t show_rh;
static uint32_t last_read_ms, last_hist_ms;

void weather_poll(uint32_t now_ms) {
	if (pending) {
		/* started at least one frame ago: the sensor needs under 10 ms */
		pending = 0;
		have_value = kit_sht_read(&cur_t, &cur_h);
		if (have_value && (hist_n == 0 || now_ms - last_hist_ms >= (uint32_t)periods_s[period_idx] * 1000U)) {
			last_hist_ms = now_ms;
			if (hist_n == WEATHER_HIST) {
				memmove(hist_t, hist_t + 1, sizeof(hist_t) - sizeof(hist_t[0]));
				memmove(hist_h, hist_h + 1, sizeof(hist_h) - sizeof(hist_h[0]));
				hist_n--;
			}
			hist_t[hist_n] = cur_t;
			hist_h[hist_n] = cur_h;
			hist_n++;
		}
		return;
	}
	if (!started || now_ms - last_read_ms >= READ_MS) {
		started = 1;
		last_read_ms = now_ms;
		pending = kit_sht_start();
		if (!pending) {
			have_value = 0;
		}
	}
}

uint8_t weather_now(int16_t* t10, uint16_t* rh10) {
	*t10 = cur_t;
	*rh10 = cur_h;
	return have_value;
}

uint8_t weather_count(void) {
	return hist_n;
}

void weather_sample(uint8_t i, int16_t* t10, uint16_t* rh10) {
	*t10 = hist_t[i];
	*rh10 = hist_h[i];
}

uint16_t weather_period_s(void) {
	return periods_s[period_idx];
}

/* "23.4" or "-5.0" from tenths */
char* weather_fmt(char* buf, int v10) {
	char num[12];
	char* p = buf;

	if (v10 < 0) {
		*p++ = '-';
		v10 = -v10;
	}
	for (const char* s = gfx_utoa(num, (uint32_t)(v10 / 10), 1); *s; ) {
		*p++ = *s++;
	}
	*p++ = '.';
	*p++ = (char)('0' + v10 % 10);
	*p = 0;
	return buf;
}

static void weather_enter(void) {
}

static void weather_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		show_rh = !show_rh;
	}
	else if (btn == KIT_BTN_2) {
		period_idx = (uint8_t)((period_idx + 1) % (sizeof(periods_s) / sizeof(periods_s[0])));
		hist_n = 0;
	}
	ui_beep(1800, 15);
}

static void graph(void) {
	int lo = 32767, hi = -32768, span;
	char buf[12];

	for (uint8_t i = 0; i < hist_n; i++) {
		int v = show_rh ? (int)hist_h[i] : hist_t[i];

		if (v < lo) { lo = v; }
		if (v > hi) { hi = v; }
	}
	/* at least 1.0 C / 5.0 % from bottom to top, so noise does not fill the graph */
	span = show_rh ? 50 : 10;
	if (hi - lo < span) {
		lo -= (span - (hi - lo)) / 2;
		hi = lo + span;
	}
	gfx_text(0, GRAPH_Y, weather_fmt(buf, hi), 1);
	gfx_text(0, GRAPH_Y + GRAPH_H - 8, weather_fmt(buf, lo), 1);
	gfx_vline(GRAPH_X - 2, GRAPH_Y, GRAPH_H, 1);

	for (uint8_t i = 0; i < hist_n; i++) {
		int v = show_rh ? (int)hist_h[i] : hist_t[i];
		int y = GRAPH_Y + GRAPH_H - 1 - (v - lo) * (GRAPH_H - 1) / (hi - lo);
		int x = GRAPH_X + (WEATHER_HIST - hist_n) + i;		/* newest at the right edge */

		if (i > 0) {
			int pv = show_rh ? (int)hist_h[i - 1] : hist_t[i - 1];
			int py = GRAPH_Y + GRAPH_H - 1 - (pv - lo) * (GRAPH_H - 1) / (hi - lo);

			gfx_line(x - 1, py, x, y, 1);
		}
		else {
			gfx_pixel(x, y, 1);
		}
	}
}

static void weather_frame(uint32_t now_ms) {
	char buf[12];
	int x;

	(void)now_ms;
	gfx_clear();
	ui_title(show_rh ? "HUMIDITY" : "TEMPERATURE", period_names[period_idx]);

	if (!have_value) {
		gfx_text_center(22, "no SHT45 answer", 1);
		ui_footer("T/RH", "RATE", "hold:MENU");
		return;
	}
	/* both values in big digits; the one in the graph is underlined */
	x = gfx_text(1, 11, weather_fmt(buf, cur_t), 2);
	gfx_rect(x + 1, 11, 4, 4, 1);							/* degree sign */
	gfx_text(x + 6, 11, "C", 1);
	x = gfx_text(70, 11, weather_fmt(buf, cur_h), 2);
	gfx_text(x + 1, 18, "%", 1);
	gfx_hline(show_rh ? 70 : 1, 27, 46, 1);

	graph();
	ui_footer("T/RH", "RATE", "hold:MENU");
}

const ui_screen_t scr_weather = { "Weather", weather_enter, weather_key, weather_frame, 0 };
