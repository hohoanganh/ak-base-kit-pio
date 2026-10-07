#include "music.h"
#include "kit.h"

/* frequencies of octave 8 (C8 .. B8); lower octaves by shifting right */
static const uint16_t freq8[12] = { 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902 };

static const char* pos;				/* next note to read, 0 = not playing */
static uint8_t def_dur, def_oct;
static uint16_t whole_ms;			/* length of a whole note */
static uint16_t gap_ms;				/* silence owed after the note that is sounding */
static uint8_t cur_note, cur_oct;
static uint16_t count;

static uint16_t number(const char** p) {
	uint16_t v = 0;

	while (**p >= '0' && **p <= '9') {
		v = (uint16_t)(v * 10 + (uint16_t)(**p - '0'));
		(*p)++;
	}
	return v;
}

/* Reads "name:d=4,o=5,b=140:" and returns the first note, or 0. */
static const char* header(const char* s, uint8_t* dur, uint8_t* oct, uint16_t* whole) {
	uint16_t bpm = 63;

	*dur = 4;
	*oct = 6;
	while (*s && *s != ':') {
		s++;
	}
	if (*s != ':') {
		return 0;
	}
	s++;
	while (*s && *s != ':') {
		char key = *s;

		if ((key == 'd' || key == 'o' || key == 'b') && s[1] == '=') {
			uint16_t v;

			s += 2;
			v = number(&s);
			if (key == 'd' && v) {
				*dur = (uint8_t)v;
			}
			else if (key == 'o' && v >= 4 && v <= 7) {
				*oct = (uint8_t)v;
			}
			else if (key == 'b' && v) {
				bpm = v;
			}
		}
		else {
			s++;
		}
	}
	if (*s != ':') {
		return 0;
	}
	*whole = (uint16_t)(240000UL / bpm);		/* four beats */
	return s + 1;
}

/* Reads one note at *p: note 0 (pause) or 1..12, its octave and length in ms.
 * Returns 0 at the end of the song. */
static uint8_t next_note(const char** p, uint8_t dur_default, uint8_t oct_default, uint16_t whole,
						 uint8_t* note, uint8_t* oct, uint16_t* ms) {
	static const uint8_t semitone[7] = { 10, 12, 1, 3, 5, 6, 8 };	/* a b c d e f g */
	const char* s = *p;
	uint16_t d;
	uint8_t dotted = 0;

	while (*s == ' ' || *s == ',') {
		s++;
	}
	if (*s == 0) {
		return 0;
	}
	d = number(&s);
	if (d == 0) {
		d = dur_default;
	}

	if (*s >= 'a' && *s <= 'g') {
		*note = semitone[*s - 'a'];
	}
	else {
		*note = 0;				/* 'p' or anything else: a pause */
	}
	if (*s) {
		s++;
	}
	if (*s == '#') {
		(*note)++;
		s++;
	}
	if (*s == '.') {
		dotted = 1;
		s++;
	}
	*oct = oct_default;
	if (*s >= '4' && *s <= '7') {
		*oct = (uint8_t)(*s - '0');
		s++;
	}
	if (*s == '.') {			/* the dot may also follow the octave */
		dotted = 1;
		s++;
	}
	while (*s && *s != ',') {	/* anything unknown: skip to the next note */
		s++;
	}

	*ms = (uint16_t)(whole / d);
	if (dotted) {
		*ms = (uint16_t)(*ms + *ms / 2);
	}
	*p = s;
	return 1;
}

uint8_t music_start(const char* rtttl) {
	music_stop();
	pos = header(rtttl, &def_dur, &def_oct, &whole_ms);
	count = 0;
	return pos != 0;
}

uint16_t music_step(void) {
	uint16_t ms;

	if (!pos) {
		return 0;
	}
	if (gap_ms) {				/* the gap that ends the note just played */
		ms = gap_ms;
		gap_ms = 0;
		kit_buzzer(0);
		return ms;
	}
	if (!next_note(&pos, def_dur, def_oct, whole_ms, &cur_note, &cur_oct, &ms)) {
		music_stop();
		return 0;
	}
	count++;
	if (cur_note == 0 || ms < 16) {
		kit_buzzer(0);
		return ms ? ms : 1;
	}
	kit_buzzer((uint16_t)(freq8[cur_note - 1] >> (8 - cur_oct)));
	gap_ms = (uint16_t)(ms / 8);
	return (uint16_t)(ms - gap_ms);
}

void music_stop(void) {
	pos = 0;
	gap_ms = 0;
	cur_note = 0;
	kit_buzzer(0);
}

uint8_t music_playing(void) {
	return pos != 0;
}

uint8_t music_note(void) {
	return cur_note;
}

uint8_t music_octave(void) {
	return cur_oct;
}

uint16_t music_count(void) {
	return count;
}

const char* music_name(const char* rtttl, char* buf, uint8_t size) {
	uint8_t n = 0;

	while (*rtttl && *rtttl != ':' && n + 1 < size) {
		buf[n++] = *rtttl++;
	}
	buf[n] = 0;
	return buf;
}

uint16_t music_measure(const char* rtttl, uint32_t* total_ms) {
	uint8_t d, o, note, oct;
	uint16_t whole, ms, n = 0;
	const char* p = header(rtttl, &d, &o, &whole);

	*total_ms = 0;
	if (!p) {
		return 0;
	}
	while (next_note(&p, d, o, whole, &note, &oct, &ms)) {
		*total_ms += ms;
		n++;
	}
	return n;
}
