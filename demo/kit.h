/**
 ******************************************************************************
 * @brief:  Board support of the AK Base Kit used by the demo: 128x64 OLED,
 *          three buttons, buzzer, RTC, SHT45, SPI flash. Implemented per port
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

/* Temperature / humidity sensor (SHT4x). kit_sht_start() begins a measurement,
 * kit_sht_read() fetches it 10 ms or more later: t10 in 0.1 C, rh10 in 0.1 %.
 * Both return 0 if the sensor does not answer (or the checksum is wrong). */
extern uint8_t kit_sht_start(void);
extern uint8_t kit_sht_read(int16_t* t10, uint16_t* rh10);

/* Media store: the part of the SPI flash the firmware update does not use.
 * Erase before writing; an erase clears one sector of KIT_STORE_SECTOR bytes
 * and can take some 100 ms. All return 1 on success. */
#define KIT_STORE_SECTOR	(4096U)
extern uint32_t kit_store_size(void);					/* bytes, 0 = no flash */
extern uint8_t kit_store_read(uint32_t off, void* buf, uint32_t len);
extern uint8_t kit_store_erase(uint32_t off);			/* off: start of a sector */
extern uint8_t kit_store_write(uint32_t off, const void* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __KIT_H__ */
