/**
 ******************************************************************************
 * @brief:  CRC32 (IEEE 802.3, same as Python zlib.crc32) and CRC16-CCITT
 *          (poly 0x1021, init 0xFFFF, same as binascii.crc_hqx(d, 0xFFFF)).
 *          16-entry nibble table: small enough for the bootloader.
 ******************************************************************************
**/

#ifndef __CRC_H__
#define __CRC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define CRC32_INIT		(0x00000000UL)
#define CRC16_INIT		(0xFFFFU)

/* Incremental: crc = crc32_update(crc32_update(0, a, n), b, m). */
extern uint32_t crc32_update(uint32_t crc, const void* data, uint32_t len);
extern uint16_t crc16_update(uint16_t crc, const void* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H__ */
