/**
 ******************************************************************************
 * @brief:  Writes a new firmware image into STAGING, independent of the
 *          transport (UART, Modbus, RF... all use the same calls).
 *
 *   fw_update_begin(total)              total = 256 (header) + binary
 *   fw_update_write(offset, data, len)  sequential, offset == bytes written
 *   fw_update_finish(&hdr)              verifies the whole image in STAGING
 *
 *  - Pages are erased lazily as writing reaches them (no long blocking
 *    erase, no watchdog risk).
 *  - len must be a multiple of the flash write_size except for the last chunk.
 *  - begin() may be called at any time to restart.
 ******************************************************************************
**/

#ifndef __FW_UPDATE_H__
#define __FW_UPDATE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "fw_types.h"
#include "fw_image.h"

typedef enum {
	FW_UPDATE_IDLE = 0,
	FW_UPDATE_RECEIVING,
	FW_UPDATE_READY,		/* verified, waiting for install */
} fw_update_state_t;

extern fw_err_t fw_update_begin(uint32_t total_size);
extern fw_err_t fw_update_write(uint32_t offset, const uint8_t* data, uint32_t len);
extern fw_err_t fw_update_finish(fw_image_hdr_t* hdr_out);
extern void fw_update_abort(void);

extern fw_update_state_t fw_update_state(void);
extern uint32_t fw_update_written(void);
extern uint32_t fw_update_total(void);

#ifdef __cplusplus
}
#endif

#endif /* __FW_UPDATE_H__ */
