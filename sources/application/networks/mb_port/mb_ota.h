#ifndef __MB_OTA_H__
#define __MB_OTA_H__

/* OTA qua Modbus: may tram thai tren khoi holding register 0xF000.
 * Thuan C - thao tac flash/commit di qua mb_ota_ops_t de test tren PC.
 * Bang thanh ghi: docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md */

#include <stdbool.h>
#include <stdint.h>
#include "nanomodbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_OTA_REG_CMD			(0xF000)
#define MB_OTA_REG_STATUS		(0xF001)
#define MB_OTA_REG_LEN_HI		(0xF002)	/* F002..F006: len hi, len lo, checksum, psk hi, psk lo */
#define MB_OTA_REG_CHECKSUM		(0xF004)
#define MB_OTA_REG_PSK_HI		(0xF005)
#define MB_OTA_REG_PSK_LO		(0xF006)
#define MB_OTA_REG_RECV_HI		(0xF007)	/* F007..F008: so byte da nhan */
#define MB_OTA_REG_RECV_LO		(0xF008)
#define MB_OTA_REG_CHUNK		(0xF010)	/* F010..F011 offset, F012.. du lieu */
#define MB_OTA_REG_LAST			(0xF051)
#define MB_OTA_CHUNK_MAX_REGS	(64)		/* 128 byte du lieu */

#define MB_OTA_CMD_BEGIN		(1)
#define MB_OTA_CMD_COMMIT		(2)
#define MB_OTA_CMD_ABORT		(3)

#define MB_OTA_ST_IDLE			(0x0000)
#define MB_OTA_ST_RECEIVING		(0x0001)
#define MB_OTA_ST_COMMITTED		(0x0002)
#define MB_OTA_ST_ERR_HEADER	(0x8001)
#define MB_OTA_ST_ERR_OFFSET	(0x8002)
#define MB_OTA_ST_ERR_CHECKSUM	(0x8003)
#define MB_OTA_ST_ERR_SIZE		(0x8004)

#define MB_OTA_PSK				(0x1A2B3C4DUL)	/* = FIRMWARE_PSK (sys_boot.h) */
#define MB_OTA_MAX_LEN			(118784UL)		/* vung app 116K */

typedef struct {
	void (*erase)(void);
	void (*write)(uint32_t offset, const uint8_t* data, uint16_t len);
	uint16_t (*checksum)(uint32_t len);
	void (*commit)(uint32_t psk, uint32_t bin_len, uint16_t checksum);	/* hen reset, KHONG reset ngay */
} mb_ota_ops_t;

extern void mb_ota_init(const mb_ota_ops_t* ops);
extern bool mb_ota_owns(uint16_t address, uint16_t quantity);
extern nmbs_error mb_ota_read(uint16_t address, uint16_t quantity, uint16_t* out);
extern nmbs_error mb_ota_write(uint16_t address, uint16_t quantity, const uint16_t* regs);

#ifdef __cplusplus
}
#endif

#endif /* __MB_OTA_H__ */
