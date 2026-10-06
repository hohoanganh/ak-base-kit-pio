/**
 ******************************************************************************
 * @brief:  Tiny monochrome graphics for the 128x64 display of the demo.
 *
 *  Everything draws into a frame buffer in RAM. gfx_flush() sends only the
 *  display pages whose content changed since the last flush, so a clock that
 *  changes a few digits costs one or two pages instead of the whole screen.
 ******************************************************************************
**/

#ifndef __GFX_H__
#define __GFX_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "kit.h"

#define GFX_W			KIT_LCD_W
#define GFX_H			KIT_LCD_H
#define GFX_FONT_W		(6)		/* 5 pixel glyph + 1 pixel gap */
#define GFX_FONT_H		(8)

extern void gfx_clear(void);
extern void gfx_pixel(int x, int y, uint8_t on);
extern uint8_t gfx_get(int x, int y);
extern void gfx_hline(int x, int y, int w, uint8_t on);
extern void gfx_vline(int x, int y, int h, uint8_t on);
extern void gfx_rect(int x, int y, int w, int h, uint8_t on);		/* outline */
extern void gfx_fill(int x, int y, int w, int h, uint8_t on);
extern void gfx_invert(int x, int y, int w, int h);

/* 5x7 font (descenders use an 8th row), scale 1 = 6x8 pixel cell. Returns the x after the text. */
extern int gfx_text(int x, int y, const char* s, uint8_t scale);
extern int gfx_text_width(const char* s, uint8_t scale);
extern int gfx_text_center(int y, const char* s, uint8_t scale);

/* Seven-segment digit 0..9 in a w x h box, segment thickness t. */
extern void gfx_digit7(int x, int y, int w, int h, int t, uint8_t digit);

/* Decimal number into buf (at least 11 chars + 0). min_digits pads with '0'.
 * Returns buf. */
extern char* gfx_utoa(char* buf, uint32_t v, uint8_t min_digits);

/* Send the pages that changed. Returns the number of pages written; force = 1
 * sends everything (first frame, display re-initialised). */
extern uint8_t gfx_flush(uint8_t force);

/* Frame buffer as the display takes it: [page][column], bit 0 = top pixel. */
extern const uint8_t* gfx_page(uint8_t page);

#ifdef __cplusplus
}
#endif

#endif /* __GFX_H__ */
