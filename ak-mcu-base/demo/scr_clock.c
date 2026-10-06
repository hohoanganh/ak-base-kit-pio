/**
 * Digital clock. Time comes from the RTC of the kit when it answers, else
 * from a soft clock counted from the millisecond tick (starts at 00:00:00).
 *   B1: choose what to set (hours -> minutes -> done)
 *   B2: +1 on the chosen field (seconds restart at 0)
 */
#include "ui.h"
#include "hal.h"

static uint8_t have_rtc;
static uint8_t hh, mm, ss;
static uint32_t soft_base_ms;		/* soft clock: hal_millis() at 00:00:00 */
static uint32_t last_read_ms;
static uint8_t set_field;			/* 0 none, 1 hours, 2 minutes */

static void soft_set(uint8_t h, uint8_t m, uint8_t s) {
	soft_base_ms = hal_millis() - ((uint32_t)h * 3600UL + (uint32_t)m * 60UL + s) * 1000UL;
}

static void time_read(void) {
	if (have_rtc && kit_rtc_get(&hh, &mm, &ss)) {
		return;
	}
	if (have_rtc) {
		/* RTC stopped answering: carry on from the last time it gave */
		have_rtc = 0;
		soft_set(hh, mm, ss);
	}
	{
		uint32_t t = ((hal_millis() - soft_base_ms) / 1000UL) % 86400UL;

		hh = (uint8_t)(t / 3600UL);
		mm = (uint8_t)((t / 60UL) % 60UL);
		ss = (uint8_t)(t % 60UL);
	}
}

static void time_write(void) {
	if (!have_rtc || !kit_rtc_set(hh, mm, ss)) {
		have_rtc = 0;
		soft_set(hh, mm, ss);
	}
}

static void clock_enter(void) {
	uint8_t h, m, s;

	set_field = 0;
	if (kit_rtc_get(&h, &m, &s)) {
		have_rtc = 1;
	}
	else if (have_rtc) {
		have_rtc = 0;
		soft_set(hh, mm, ss);
	}
	last_read_ms = hal_millis() - 1000;
}

static void clock_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		set_field = (uint8_t)((set_field + 1) % 3);
		ui_beep(1800, 15);
	}
	else if (btn == KIT_BTN_2 && set_field) {
		time_read();
		if (set_field == 1) {
			hh = (uint8_t)((hh + 1) % 24);
		}
		else {
			mm = (uint8_t)((mm + 1) % 60);
		}
		ss = 0;
		time_write();
		ui_beep(2200, 15);
	}
}

static void clock_frame(uint32_t now_ms) {
	char buf[12];
	uint8_t blink = (uint8_t)((now_ms / 250U) & 1U);
	const int dw = 20, dh = 38, t = 5, y = 12;

	if (now_ms - last_read_ms >= 200) {		/* a few reads per second: the display never lags a full second */
		last_read_ms = now_ms;
		time_read();
	}

	gfx_clear();
	ui_title("CLOCK", have_rtc ? "RTC" : "soft");

	/* HH:MM in seven-segment digits; the field being set blinks */
	if (!(set_field == 1 && blink)) {
		gfx_digit7(2, y, dw, dh, t, hh / 10);
		gfx_digit7(26, y, dw, dh, t, hh % 10);
	}
	if ((ss & 1) || set_field) {
		gfx_fill(50, y + 10, 5, 5, 1);
		gfx_fill(50, y + 23, 5, 5, 1);
	}
	if (!(set_field == 2 && blink)) {
		gfx_digit7(59, y, dw, dh, t, mm / 10);
		gfx_digit7(83, y, dw, dh, t, mm % 10);
	}
	gfx_text(110, y + dh - 7, gfx_utoa(buf, ss, 2), 1);

	ui_footer(set_field == 0 ? "SET" : (set_field == 1 ? "HOUR" : "MIN"),
			  set_field ? "+1" : 0, "hold:MENU");
}

const ui_screen_t scr_clock = { "Digital clock", clock_enter, clock_key, clock_frame, 0 };
