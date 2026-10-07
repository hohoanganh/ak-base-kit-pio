/**
 ******************************************************************************
 * @brief:  Samples for the scope screen (scr_plot.c). Whoever has a number to
 *          show calls plot_add(): the shell command "plot", the Modbus slave
 *          when register 16 is written, or code of your own.
 ******************************************************************************
**/

#ifndef __PLOT_H__
#define __PLOT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define PLOT_POINTS		(120)		/* points across the screen */

/* One more point at the right edge of the graph. */
extern void plot_add(int16_t v);

/* Points on screen now. */
extern uint8_t plot_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __PLOT_H__ */
