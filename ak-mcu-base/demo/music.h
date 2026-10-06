/**
 ******************************************************************************
 * @brief:  Tune player for the buzzer. Songs are RTTTL strings
 *          ("name:d=4,o=5,b=140:e,e,f,g,..."), the ringtone text format.
 *
 *  The player never waits. music_step() starts the next piece of the song on
 *  the buzzer and returns how long it lasts; the caller comes back after that
 *  time (the demo uses a one-shot kernel timer, see ui_music_play()).
 *  Every note is sounded for 7/8 of its length and followed by a short gap,
 *  so repeated notes can be told apart.
 ******************************************************************************
**/

#ifndef __MUSIC_H__
#define __MUSIC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* Returns 1 if the header of the song could be read. */
extern uint8_t music_start(const char* rtttl);

/* Next piece: sets the buzzer, returns its duration in ms. 0 = song finished
 * (buzzer off). */
extern uint16_t music_step(void);

extern void music_stop(void);
extern uint8_t music_playing(void);

/* Note sounding now: 0 = pause, 1..12 = C, C#, D ... B. */
extern uint8_t music_note(void);
extern uint8_t music_octave(void);
/* Number of notes started so far (changes when a new note begins). */
extern uint16_t music_count(void);

/* Song name (text before the first ':') into buf, at most size - 1 chars. */
extern const char* music_name(const char* rtttl, char* buf, uint8_t size);

/* Whole song: number of notes and length in ms (does not play it). */
extern uint16_t music_measure(const char* rtttl, uint32_t* total_ms);

#ifdef __cplusplus
}
#endif

#endif /* __MUSIC_H__ */
