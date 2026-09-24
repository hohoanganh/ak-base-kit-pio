#ifndef __TASK_FW_H__
#define __TASK_FW_H__

#include <stdint.h>

#include "sys_boot.h"

extern void fw_ext_erase(void);
extern void fw_ext_write(uint32_t offset, const uint8_t* data, uint16_t len);
extern uint16_t fw_ext_checksum(uint32_t len);
/* Luu header, hen FW_MB_OTA_COMMIT sau 200 ms (de phan hoi Modbus kip ra day)
 * roi moi ghi BSF + reset. */
extern void fw_commit_app_later(const firmware_header_t* header);

#endif //__TASK_FW_H__
