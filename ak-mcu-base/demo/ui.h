/**
 ******************************************************************************
 * @brief:  Demo for the AK Base Kit: menu, digital clock, two games and a
 *          system monitor on the 128x64 display, three buttons, buzzer.
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
};

typedef struct {
	const char* name;						/* menu entry */
	void (*enter)(void);					/* screen opened */
	void (*key)(uint8_t btn);				/* KIT_BTN_1 / _2 / _3, short press */
	void (*frame)(uint32_t now_ms);			/* every UI_FRAME_MS: update + draw */
} ui_screen_t;

extern const ui_screen_t scr_clock;
extern const ui_screen_t scr_snake;
extern const ui_screen_t scr_flappy;
extern const ui_screen_t scr_system;

/* menu (scr_menu.c): returns the chosen screen on B3, else 0 */
extern void menu_enter(void);
extern const ui_screen_t* menu_key(uint8_t btn);
extern void menu_frame(uint32_t now_ms);

/* helpers for the screens */
extern void ui_beep(uint16_t freq_hz, uint16_t ms);
extern uint32_t ui_rand(void);
extern void ui_footer(const char* b1, const char* b2, const char* b3);
extern void ui_title(const char* left, const char* right);

/* frame statistics of the last flush, shown by the system screen */
extern uint8_t ui_last_pages;
extern uint16_t ui_last_frame_ms;

/* shell: "ui" = status, "ui 1|2|3" = press a button, "ui back" = hold B3 */
extern void cmd_ui(const char* args);

extern void task_ui(ak_msg_t* msg);
extern void task_poll_buttons(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_H__ */
