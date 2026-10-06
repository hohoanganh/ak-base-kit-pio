/**
 ******************************************************************************
 * @brief:  Demo for the AK Base Kit: menu, digital clock, two games and a
 *          system monitor on the 128x64 display, three buttons, buzzer.
 *          Later additions: Dino runner, rotating 3D solids, a jukebox,
 *          a ray-cast maze, video from the SPI flash, a weather station.
 *
 *  How it sits on the kernel:
 *   - task_poll_buttons (polling) debounces the buttons and posts key signals;
 *   - task_ui owns the screen: a periodic timer posts UI_SIG_FRAME, the
 *     active screen updates and draws into the frame buffer, gfx_flush()
 *     sends the pages that changed;
 *   - a beep is "buzzer on" + a one-shot timer that posts UI_SIG_BEEP_OFF.
 *  Nothing waits: every handler returns within one frame.
 *
 *  Buttons (left to right under the display): B1, B2, B3.
 *   menu:    B1 down, B2 up, B3 open
 *   screens: B1 / B2 as written in the footer, B3 short = third action,
 *            B3 held = back to the menu
 ******************************************************************************
**/

#ifndef __UI_H__
#define __UI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "ak.h"
#include "message.h"
#include "kit.h"
#include "gfx.h"

#define UI_FRAME_MS			(50)		/* 20 frames per second */
#define UI_HOLD_MS			(700)		/* B3 held this long = back to the menu */

/* TASK_UI_ID */
enum {
	UI_SIG_INIT = AK_USER_DEFINE_SIG,
	UI_SIG_FRAME,
	UI_SIG_KEY_1,			/* B1 pressed */
	UI_SIG_KEY_2,			/* B2 pressed */
	UI_SIG_KEY_3,			/* B3 released before UI_HOLD_MS */
	UI_SIG_BACK,			/* B3 held */
	UI_SIG_BEEP_OFF,
	UI_SIG_NOTE,			/* music: the current piece of the song is over */
};

typedef struct {
	const char* name;						/* menu entry */
	void (*enter)(void);					/* screen opened */
	void (*key)(uint8_t btn);				/* KIT_BTN_1 / _2 / _3, short press */
	void (*frame)(uint32_t now_ms);			/* every UI_FRAME_MS: update + draw */
	void (*leave)(void);					/* screen closed (may be 0) */
} ui_screen_t;

extern const ui_screen_t scr_clock;
extern const ui_screen_t scr_snake;
extern const ui_screen_t scr_flappy;
extern const ui_screen_t scr_dino;
extern const ui_screen_t scr_cube;
extern const ui_screen_t scr_music;
extern const ui_screen_t scr_maze;
extern const ui_screen_t scr_video;
extern const ui_screen_t scr_weather;
extern const ui_screen_t scr_system;

/* menu (scr_menu.c): returns the chosen screen on B3, else 0 */
extern void menu_enter(void);
extern const ui_screen_t* menu_key(uint8_t btn);
extern void menu_frame(uint32_t now_ms);

/* helpers for the screens */
extern void ui_beep(uint16_t freq_hz, uint16_t ms);		/* silent while a song plays */
/* play an RTTTL song on the buzzer without blocking (music.h) / stop it */
extern void ui_music_play(const char* rtttl);
extern void ui_music_stop(void);
extern uint32_t ui_rand(void);
/* 127 * sin / cos of an angle in 1/256 of a turn (table, no floating point) */
extern int ui_sin(uint8_t a);
extern int ui_cos(uint8_t a);
extern void ui_footer(const char* b1, const char* b2, const char* b3);
extern void ui_title(const char* left, const char* right);

/* 1: the games play themselves (shell "ui auto", also used to record the
 * pictures in the documentation). Any button gives control back. */
extern uint8_t ui_autoplay;

/* frame statistics of the last flush, shown by the system screen */
extern uint8_t ui_last_pages;
extern uint16_t ui_last_frame_ms;

/* shell: "ui" = status, "ui 1|2|3" = press a button, "ui back" = hold B3,
 * "ui auto" = games play themselves */
extern void cmd_ui(const char* args);
/* shell: "th" = temperature, humidity and the graph of the weather screen as CSV */
extern void cmd_th(const char* args);
/* fw_proto extension (console): commands 0x40.. load files into the media
 * store, see tools/ak_video.py. Returns the status byte, fills the response. */
extern uint8_t ui_proto_ext(uint8_t cmd, const uint8_t* req, uint16_t len, uint8_t* resp, uint16_t* resp_len);

extern void task_ui(ak_msg_t* msg);
extern void task_poll_buttons(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_H__ */
