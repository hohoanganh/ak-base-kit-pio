/**
 * Jukebox: tunes on the buzzer, with the notes scrolling by as bars.
 *   B1: next song
 *   B2: play / stop
 * The songs are RTTTL strings (see music.h). All of them are traditional or
 * long out of copyright.
 */
#include <string.h>

#include "ui.h"
#include "music.h"

static const char* const songs[] = {
	"Ode to Joy:d=4,o=5,b=140:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d,e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c,"
		"d,d,e,c,d,8e,8f,e,c,d,8e,8f,e,d,c,d,2g4,e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c",
	"Fur Elise:d=8,o=5,b=125:e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,g#,b,4c6,p,e,e6,d#6,"
		"e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,c6,b,2a",
	"Korobeiniki:d=4,o=5,b=150:e6,8b,8c6,d6,8c6,8b,a,8a,8c6,e6,8d6,8c6,b.,8c6,d6,e6,c6,a,2a,8p,"
		"d6,8f6,a6,8g6,8f6,e6.,8c6,e6,8d6,8c6,b,8b,8c6,d6,e6,c6,a,a",
	"Entertainer:d=8,o=5,b=140:d,d#,e,4c6,e,4c6,e,2c6.,c6,d6,d#6,e6,c6,d6,4e6,b,4d6,2c6,p,"
		"d,d#,e,4c6,e,4c6,e,2c6.,p,a,g,f#,a,c6,4e6,d6,c6,a,2d6",
	"Twinkle:d=4,o=5,b=120:c,c,g,g,a,a,2g,f,f,e,e,d,d,2c,g,g,f,f,e,e,2d,g,g,f,f,e,e,2d,"
		"c,c,g,g,a,a,2g,f,f,e,e,d,d,2c",
	"Happy Birthday:d=4,o=5,b=120:8c.,16c,d,c,f,2e,8c.,16c,d,c,g,2f,8c.,16c,c6,a,f,e,d,"
		"8a#.,16a#,a,f,g,2f",
};

#define SONG_NUM	((uint8_t)(sizeof(songs) / sizeof(songs[0])))
#define BARS		(40)		/* notes kept on screen, 3 pixels each */

static uint8_t song;			/* selected in the list */
static uint8_t now_playing;		/* the one that sounds */
static uint8_t bars[BARS];		/* 0 = pause, else height in pixels; newest last */
static uint16_t seen_count;
static uint16_t song_notes;

static void bars_clear(void) {
	memset(bars, 0, sizeof(bars));
	seen_count = 0;
}

static void play(void) {
	uint32_t ms;

	bars_clear();
	now_playing = song;
	song_notes = music_measure(songs[song], &ms);
	ui_music_play(songs[song]);
}

static void music_enter(void) {
	bars_clear();
}

static void music_leave(void) {
	ui_music_stop();
}

static void music_key(uint8_t btn) {
	if (btn == KIT_BTN_1) {
		uint8_t was_playing = music_playing();

		song = (uint8_t)((song + 1) % SONG_NUM);
		ui_music_stop();
		bars_clear();
		if (was_playing) {
			play();
		}
	}
	else if (btn == KIT_BTN_2) {
		if (music_playing()) {
			ui_music_stop();
		}
		else {
			play();
		}
	}
}

static void music_frame(uint32_t now_ms) {
	static const char* const names[13] = { "", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
	char buf[24];
	uint8_t playing = music_playing();

	(void)now_ms;
	if (ui_autoplay && !playing) {
		play();					/* attract mode: one song after the other */
		song = (uint8_t)((song + 1) % SONG_NUM);
		playing = music_playing();
	}

	/* one bar per note: height = pitch (C4 lowest .. B7 highest) */
	while (playing && seen_count != music_count()) {
		uint8_t n = music_note();
		uint8_t h = n ? (uint8_t)(2 + ((music_octave() - 4) * 12 + n) * 24 / 48) : 0;

		memmove(bars, bars + 1, BARS - 1);
		bars[BARS - 1] = h;
		seen_count++;
	}

	gfx_clear();
	{
		uint8_t shown = playing ? now_playing : song;
		char num[4];

		ui_title("MUSIC", gfx_utoa(num, (uint32_t)shown + 1, 1));
		gfx_text_center(11, music_name(songs[shown], buf, sizeof(buf)), 1);
	}

	for (uint8_t i = 0; i < BARS; i++) {
		if (bars[i]) {
			gfx_fill(4 + i * 3, 47 - bars[i], 2, bars[i], 1);
		}
		else {
			gfx_pixel(4 + i * 3, 47, 1);
		}
	}
	/* how far into the song */
	gfx_rect(4, 50, 120, 3, 1);
	if (playing && song_notes) {
		gfx_fill(4, 50, (int)((uint32_t)music_count() * 120U / song_notes), 3, 1);
	}

	if (playing && music_note()) {
		char* p = buf;
		const char* nm = names[music_note()];

		while (*nm) {
			*p++ = *nm++;
		}
		*p++ = (char)('0' + music_octave());
		*p = 0;
		gfx_text(GFX_W - 2 - gfx_text_width(buf, 1), 21, buf, 1);
	}

	ui_footer("NEXT", playing ? "STOP" : "PLAY", "hold:MENU");
}

const ui_screen_t scr_music = { "Jukebox", music_enter, music_key, music_frame, music_leave };
