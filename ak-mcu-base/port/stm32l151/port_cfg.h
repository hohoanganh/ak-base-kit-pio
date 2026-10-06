/**
 ******************************************************************************
 * @brief:  STM32L151CB port configuration (AK Embedded Base Kit).
 *
 *  Flash 128K @ 0x08000000, 256 B pages, erased value 0x00.
 *  RAM 16K @ 0x20000000. Data EEPROM 4K @ 0x08080000.
 *  External W25Qxx SPI NOR on SPI1 (default STAGING location).
 *
 *  PORT_STAGING_EXTERNAL = 1 (default):
 *    0x08000000  BOOT     12K  bootloader
 *    0x08003000  APP     116K  [header 256 B][app, vector @ 0x08003100]
 *    NOR 0x80000 STAGING 116K  OTA image waiting for install (4K sectors)
 *
 *  PORT_STAGING_EXTERNAL = 0 (board without SPI flash):
 *    0x08000000  BOOT     12K
 *    0x08003000  APP      58K
 *    0x08011800  STAGING  58K  internal flash
 *
 *  The app linker script (app.ld) gets the APP size from the build system
 *  (--defsym __app_part_size), see port.cmake / pio_mcu_base.py.
 ******************************************************************************
**/

#ifndef __PORT_CFG_H__
#define __PORT_CFG_H__

#define PORT_BOARD_NAME			"ak-l151"

#define PORT_FLASH_PAGE			(256UL)
#define PORT_BOOT_ADDR			(0x08000000UL)
#define PORT_BOOT_SIZE			(0x3000UL)
#define PORT_APP_ADDR			(0x08003000UL)

#ifndef PORT_STAGING_EXTERNAL
#define PORT_STAGING_EXTERNAL	(1)
#endif

#if PORT_STAGING_EXTERNAL
#define PORT_APP_SIZE			(0x1D000UL)		/* 116K: rest of internal flash */
#define PORT_STAGING_ADDR		(0x00080000UL)	/* SPI NOR offset (same as ak-base-kit) */
#define PORT_STAGING_SIZE		(0x1D000UL)
#else
#define PORT_APP_SIZE			(0xE800UL)
#define PORT_STAGING_ADDR		(0x08011800UL)
#define PORT_STAGING_SIZE		(0xE800UL)
#endif

#define PORT_RAM_START			(0x20000000UL)
#define PORT_RAM_END			(0x20004000UL)

#define PORT_EEPROM_ADDR		(0x08080000UL)	/* boot_ctrl at EEPROM start */

/* Console USART1: PA9 TX, PA10 RX */
#define PORT_CONSOLE_BAUD		(115200)
#define PORT_CONSOLE_RX_BUF		(256)		/* power of 2 */
#define PORT_CONSOLE_TX_BUF		(256)		/* power of 2, app only (boot TX is polled) */

/* SPI NOR: SPI1 PA5/PA6/PA7, CS PB14. */
#define PORT_NOR_CS_PORT		GPIOB
#define PORT_NOR_CS_PIN			GPIO_Pin_14
#define PORT_NOR_CS_CLK			RCC_AHBPeriph_GPIOB

/* AK Base Kit only: an nRF24 module in the kit socket shares SPI1, its CSN
 * (PB9) must be held high or it answers on MISO. A product board has no such
 * device: leave 0 and PB9 stays untouched. */
#ifndef PORT_KIT_NRF24_CSN
#define PORT_KIT_NRF24_CSN		(0)
#endif
#if PORT_KIT_NRF24_CSN
#define PORT_NRF_CSN_PORT		GPIOB
#define PORT_NRF_CSN_PIN		GPIO_Pin_9
#define PORT_NRF_CSN_CLK		RCC_AHBPeriph_GPIOB
#endif
#define PORT_NOR_SPI_PRESCALER	SPI_BaudRatePrescaler_8	/* 32 MHz / 8 = 4 MHz, as ak-base-kit */

/* LED life PB8, active high */
#define PORT_LED_PORT			GPIOB
#define PORT_LED_PIN			GPIO_Pin_8
#define PORT_LED_CLK			RCC_AHBPeriph_GPIOB

#endif /* __PORT_CFG_H__ */
