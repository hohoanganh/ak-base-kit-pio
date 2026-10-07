/**
 ******************************************************************************
 * @brief:  Firmware update over Modbus: a state machine on the holding
 *          register block 0xF000, same addresses and sequence as the OTA of
 *          the older ak-base-kit, carrying an ak-mcu-base image (.img).
 *
 *   reg            R/W  meaning
 *   F000           W    CMD: 1 BEGIN, 2 COMMIT, 3 ABORT
 *   F001           R    STATUS: 0 idle, 1 receiving, 2 committed (reset follows),
 *                       0x8001 header, 0x8002 offset, 0x8003 image CRC, 0x8004 size
 *   F002..F003     R/W  total image size in bytes (hi, lo) = the whole .img file
 *   F004           R/W  not used by this image type (write 0)
 *   F005..F006     R/W  image type: MB_OTA_PSK_IMG (hi, lo)
 *   F007..F008     R    bytes received (hi, lo)
 *   F009           R    fw_err_t of the last failure (detail for the status)
 *   F010..F011     W    chunk offset (hi, lo), written together with the data
 *   F012..F051     W    chunk data, up to 64 registers = 128 bytes, high byte first
 *
 *  Sequence: write F002..F006, CMD = BEGIN, chunks in order, CMD = COMMIT.
 *  COMMIT verifies the whole image in STAGING (header CRC, board name, load
 *  address, image CRC, vector table); only then on_commit() is called and the
 *  status becomes "committed". Nothing touches the running app before that.
 *  A chunk or COMMIT sent again because the answer was lost is accepted.
 *  The image type of the older base (0x1A2B3C4D) is refused with status 0x8001.
 ******************************************************************************
**/

#ifndef __MB_OTA_H__
#define __MB_OTA_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "nanomodbus.h"

#define MB_OTA_REG_CMD			(0xF000)
#define MB_OTA_REG_STATUS		(0xF001)
#define MB_OTA_REG_LEN_HI		(0xF002)
#define MB_OTA_REG_LEN_LO		(0xF003)
#define MB_OTA_REG_CHECKSUM		(0xF004)
#define MB_OTA_REG_PSK_HI		(0xF005)
#define MB_OTA_REG_PSK_LO		(0xF006)
#define MB_OTA_REG_RECV_HI		(0xF007)
#define MB_OTA_REG_RECV_LO		(0xF008)
#define MB_OTA_REG_DETAIL		(0xF009)
#define MB_OTA_REG_CHUNK		(0xF010)
#define MB_OTA_REG_LAST			(0xF051)
#define MB_OTA_CHUNK_MAX_REGS	(64)

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

/* image type of ak-mcu-base = FW_IMAGE_MAGIC ("AKFW") */
#define MB_OTA_PSK_IMG			(0x57464B41UL)

/* on_commit: a verified image is in STAGING; arrange the install + reset AFTER
 * returning (the Modbus answer still has to go out). NULL disables OTA. */
extern void mb_ota_init(void (*on_commit)(void));

extern uint8_t mb_ota_owns(uint16_t address, uint16_t quantity);
extern nmbs_error mb_ota_read(uint16_t address, uint16_t quantity, uint16_t* out);
extern nmbs_error mb_ota_write(uint16_t address, uint16_t quantity, const uint16_t* regs);

#ifdef __cplusplus
}
#endif

#endif /* __MB_OTA_H__ */
