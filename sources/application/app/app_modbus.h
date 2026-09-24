#ifndef __APP_MODBUS_H__
#define __APP_MODBUS_H__

#include <stdint.h>
#include "nanomodbus.h"

#define APP_MB_BAUDRATE				(9600)
#define APP_MB_SLAVE_ADDR			(1)
#define APP_MB_READ_TIMEOUT_MS		(500)	/* master: cho phan hoi */
#define APP_MB_BYTE_TIMEOUT_MS		(20)	/* khoang lang toi da giua 2 byte trong 1 khung */

extern nmbs_t app_mb;

extern nmbs_error app_modbus_init(void);
extern void app_modbus_poll(void);	/* slave: goi tu task_polling_mbslave(); master: rong */

#endif /* __APP_MODBUS_H__ */
