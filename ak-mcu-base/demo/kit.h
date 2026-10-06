/**
 ******************************************************************************
 * @brief:  Board support of the AK Base Kit used by the demo: 128x64 OLED,
 *          three buttons, buzzer, RTC. Implemented per port
 *          (port/stm32l151/kit.c); the demo code above it is chip independent
 *          and also runs in the host renderer (tests/test_demo.c).
 ******************************************************************************
**/

#ifndef __KIT_H__
#define __KIT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define KIT_LCD_W			(128)
#define KIT_LCD_H			(64)
#define KIT_LCD_PAGES		(KIT_LCD_H / 8)

/* buttons below the display, left to right */
#define KIT_BTN_1			(0x01)
#define KIT_BTN_2			(0x02)
#define KIT_BTN_3			(0x04)

/* GPIO, buttons, buzzer; resets and starts the display. Returns 1 if the
 * display answered on its bus. */
extern uint8_t kit_init(void);

/* One display page: 128 bytes, one per column, bit 0 = top pixel of the page.
 * Returns 0 if the display did not acknowledge. */
extern uint8_t kit_lcd_write_page(uint8_t page, const uint8_t* data);

/* Buttons held down right now (KIT_BTN_* mask, not debounced). */
extern uint8_t kit_buttons(void);

/* Square wave on the buzzer, 0 = silent. */
extern void kit_buzzer(uint16_t freq_hz);

/* RTC (hours 0..23). Return 1 on success, 0 if no RTC answers. */
extern uint8_t kit_rtc_get(uint8_t* hh, uint8_t* mm, uint8_t* ss);
extern uint8_t kit_rtc_set(uint8_t hh, uint8_t mm, uint8_t ss);

#ifdef __cplusplus
}
#endif

#endif /* __KIT_H__ */
