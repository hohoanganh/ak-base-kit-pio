/**
 ******************************************************************************
 * @brief:  Temperature / humidity sampling behind the weather screen
 *          (scr_weather.c). Values are in tenths: 234 = 23.4 C, 652 = 65.2 %.
 ******************************************************************************
**/

#ifndef __WEATHER_H__
#define __WEATHER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define WEATHER_HIST		(96)		/* points of the graph */

/* Call every UI frame: reads the sensor once a second without waiting. */
extern void weather_poll(uint32_t now_ms);

/* Last reading. Returns 0 if the sensor does not answer. */
extern uint8_t weather_now(int16_t* t10, uint16_t* rh10);

/* The graph: weather_count() samples, 0 = oldest, one every weather_period_s(). */
extern uint8_t weather_count(void);
extern void weather_sample(uint8_t i, int16_t* t10, uint16_t* rh10);
extern uint16_t weather_period_s(void);

/* Tenths as text: 234 -> "23.4". buf: at least 8 chars. Returns buf. */
extern char* weather_fmt(char* buf, int v10);

#ifdef __cplusplus
}
#endif

#endif /* __WEATHER_H__ */
