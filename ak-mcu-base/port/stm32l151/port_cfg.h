/**
 ******************************************************************************
 * @brief:  STM32L151CB port configuration (AK Embedded Base Kit).
 *
 *  Flash 128K @ 0x08000000, 256 B pages, erased value 0x00.
 *  RAM 16K @ 0x20000000. Data EEPROM 4K @ 0x08080000.
 *
 *    0x08000000  BOOT     12K  bootloader
 *    0x08003000  APP      58K  [header 256 B][app, vector @ 0x08003100]
 *    0x08011800  STAGING  58K  OTA image waiting for install
 *
 *  Changing this map? Update MEMORY in boot.ld / app.ld as well.
 ******************************************************************************
**/

#ifndef __PORT_CFG_H__
#define __PORT_CFG_H__

#define PORT_BOARD_NAME			"ak-l151"

#define PORT_FLASH_PAGE			(256UL)
#define PORT_BOOT_ADDR			(0x08000000UL)
#define PORT_BOOT_SIZE			(0x3000UL)
#define PORT_APP_ADDR			(0x08003000UL)
#define PORT_APP_SIZE			(0xE800UL)
#define PORT_STAGING_ADDR		(0x08011800UL)
#define PORT_STAGING_SIZE		(0xE800UL)

#define PORT_RAM_START			(0x20000000UL)
#define PORT_RAM_END			(0x20004000UL)

#define PORT_EEPROM_ADDR		(0x08080000UL)	/* boot_ctrl at EEPROM start */

/* Console USART1: PA9 TX, PA10 RX */
#define PORT_CONSOLE_BAUD		(115200)
#define PORT_CONSOLE_RX_BUF		(256)		/* power of 2 */

/* LED life PB8, active high */
#define PORT_LED_PORT			GPIOB
#define PORT_LED_PIN			GPIO_Pin_8
#define PORT_LED_CLK			RCC_AHBPeriph_GPIOB

#endif /* __PORT_CFG_H__ */
