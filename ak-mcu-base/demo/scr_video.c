/**
 * Video: plays the clip stored in the SPI flash of the kit, full screen.
 *   B1: pause / play
 *   B2: back to the start
 * Load a clip with tools/ak_video.py (convert, then upload).
 *
 * A frame of the clip only says what changed, so this screen does not clear
 * and redraw like the others: the decoder patches the frame buffer in place.
 */
#include "ui.h"
#include "video.h"

static uint8_t have_clip;
static uint8_t playing;
static uint8_t restart;				/* start the clock at the next frame */
static uint32_t start_ms;			/* time of frame 0 */

static void video_enter(void) {
	have_clip = video_open(kit_store_read, kit_store_size());
	playing = have_clip;
	restart = 1;
	gfx_clear();
}

static void video_key(uint8_t btn) {
	if (!have_clip) {
		return;
	}
	if (btn == KIT_BTN_1) {
		playing = !playing;
		restart = 1;				/* continue from the frame on screen */
	}
	else if (btn == KIT_BTN_2) {
		video_rewind();
		playing = 1;
		restart = 1;
	}
	ui_beep(1800, 15);
}

static void video_frame(uint32_t now_ms) {
	uint32_t want;
	uint8_t budget = 4;				/* frames decoded per call at most */

	if (!have_clip) {
		gfx_clear();
		ui_title("VIDEO", 0);
		gfx_text_center(16, "no clip in flash", 1);
		gfx_text_center(30, "PC: ak_video.py", 1);
		gfx_text_center(40, "convert + upload", 1);
		ui_footer(0, 0, "hold:MENU");
		return;
	}
	if (!playing) {
		return;
	}
	if (restart) {
		restart = 0;
		start_ms = now_ms - (uint32_t)video_pos() * 1000U / video_fps();
	}
	/* the frame that should be on screen now; decode up to it */
	want = (now_ms - start_ms) * video_fps() / 1000U + 1;
	while (video_pos() < want && budget--) {
		if (!video_next(gfx_fb())) {
			video_rewind();			/* end of the clip (or damaged data): loop */
			start_ms = now_ms;
			want = 1;
			if (!video_next(gfx_fb())) {
				have_clip = 0;
				return;
			}
		}
	}
	if (video_pos() < want) {		/* fell behind (slow display): do not race to catch up */
		start_ms = now_ms - (uint32_t)video_pos() * 1000U / video_fps();
	}
}

const ui_screen_t scr_video = { "Video", video_enter, video_key, video_frame, 0 };
