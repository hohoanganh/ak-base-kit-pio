/**
 * Kernel side of the demo UI: the UI task, button polling, beeps.
 * See ui.h for the overall picture.
 */
#include "ak.h"
#include "task.h"
#include "timer.h"

#include "hal.h"
#include "ak_log.h"

#include "ui.h"
#include "task_list.h"

#define TAG "UI"

#define BTN_DEBOUNCE_MS		(15)

uint8_t ui_last_pages;
uint16_t ui_last_frame_ms;

static const ui_screen_t* current;		/* 0 = menu */
static uint8_t lcd_ok;

/*----------------------------------------------------------------------------
 * beep: buzzer on now, a one-shot timer turns it off
 *--------------------------------------------------------------------------*/
void ui_beep(uint16_t freq_hz, uint16_t ms) {
	kit_buzzer(freq_hz);
	timer_set(TASK_UI_ID, UI_SIG_BEEP_OFF, ms, TIMER_ONE_SHOT);
}

/*----------------------------------------------------------------------------
 * buttons: polled from the main loop, debounced, turned into messages.
 * B1 / B2 report on press (games react at once). B3 reports on release as a
 * short press, or as BACK once it has been held for UI_HOLD_MS.
 *--------------------------------------------------------------------------*/
void task_poll_buttons(void) {
	static uint32_t last_ms;
	static uint8_t raw, stable;
	static uint32_t b3_down_ms;
	static uint8_t b3_long_sent;
	uint32_t now = hal_millis();
	uint8_t sample, pressed, released;

	if (now - last_ms < BTN_DEBOUNCE_MS) {
		return;
	}
	last_ms = now;

	sample = kit_buttons();
	if (sample != raw) {		/* still bouncing: wait for two equal samples */
		raw = sample;
		return;
	}
	pressed = (uint8_t)(sample & ~stable);
	released = (uint8_t)(stable & ~sample);
	stable = sample;
	if (pressed & (KIT_BTN_1 | KIT_BTN_2)) {
		ui_autoplay = 0;		/* a player took over */
	}

	if (pressed & KIT_BTN_1) {
		task_post_pure_msg(TASK_UI_ID, UI_SIG_KEY_1);
	}
	if (pressed & KIT_BTN_2) {
		task_post_pure_msg(TASK_UI_ID, UI_SIG_KEY_2);
	}
	if (pressed & KIT_BTN_3) {
		b3_down_ms = now;
		b3_long_sent = 0;
	}
	if ((stable & KIT_BTN_3) && !b3_long_sent && now - b3_down_ms >= UI_HOLD_MS) {
		b3_long_sent = 1;
		task_post_pure_msg(TASK_UI_ID, UI_SIG_BACK);
	}
	if ((released & KIT_BTN_3) && !b3_long_sent) {
		task_post_pure_msg(TASK_UI_ID, UI_SIG_KEY_3);
	}
}

/*----------------------------------------------------------------------------
 * shell: look at the demo and press its buttons over the console
 *--------------------------------------------------------------------------*/
void cmd_ui(const char* args) {
	if (args[0] >= '1' && args[0] <= '3' && args[1] == 0) {
		task_post_pure_msg(TASK_UI_ID, (uint8_t)(UI_SIG_KEY_1 + (args[0] - '1')));
	}
	else if (args[0] == 'b') {
		task_post_pure_msg(TASK_UI_ID, UI_SIG_BACK);
	}
	else if (args[0] == 'a') {
		ui_autoplay = !ui_autoplay;
		xprintf("autoplay %s\n", ui_autoplay ? "on: open a game, it plays itself" : "off");
	}
	else {
		xprintf("display %s, screen: %s, last frame %d ms, %d of %d pages sent, buttons 0x%02X\n",
				lcd_ok ? "ok" : "NOT ANSWERING", current ? current->name : "menu",
				ui_last_frame_ms, ui_last_pages, KIT_LCD_PAGES, kit_buttons());
	}
}

/*----------------------------------------------------------------------------
 * UI task
 *--------------------------------------------------------------------------*/
static void open_screen(const ui_screen_t* s) {
	current = s;
	if (s) {
		s->enter();
	}
	else {
		menu_enter();
	}
}

static void key(uint8_t btn) {
	if (current) {
		current->key(btn);
	}
	else {
		const ui_screen_t* chosen = menu_key(btn);
		if (chosen) {
			open_screen(chosen);
		}
	}
}

void task_ui(ak_msg_t* msg) {
	uint32_t t0;

	switch (msg->sig) {
	case UI_SIG_INIT:
		lcd_ok = kit_init();
		if (!lcd_ok) {
			LOG_W(TAG, "display does not answer, demo runs without it\n");
		}
		open_screen(0);
		gfx_clear();
		gfx_flush(1);
		timer_set(TASK_UI_ID, UI_SIG_FRAME, UI_FRAME_MS, TIMER_PERIODIC);
		ui_beep(2000, 60);
		LOG_I(TAG, "demo started: %d ms per frame\n", UI_FRAME_MS);
		break;

	case UI_SIG_FRAME:
		t0 = hal_millis();
		if (current) {
			current->frame(t0);
		}
		else {
			menu_frame(t0);
		}
		ui_last_pages = lcd_ok ? gfx_flush(0) : 0;
		ui_last_frame_ms = (uint16_t)(hal_millis() - t0);
		break;

	case UI_SIG_KEY_1:
		key(KIT_BTN_1);
		break;

	case UI_SIG_KEY_2:
		key(KIT_BTN_2);
		break;

	case UI_SIG_KEY_3:
		key(KIT_BTN_3);
		break;

	case UI_SIG_BACK:
		if (current) {
			ui_beep(1200, 40);
			open_screen(0);
		}
		break;

	case UI_SIG_BEEP_OFF:
		kit_buzzer(0);
		break;

	default:
		break;
	}
}
