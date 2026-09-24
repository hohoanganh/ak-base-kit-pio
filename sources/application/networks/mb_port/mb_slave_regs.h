#ifndef __MB_SLAVE_REGS_H__
#define __MB_SLAVE_REGS_H__

/* Bang thanh ghi cua Modbus slave (TASK_MBSLAVE_EN). Thuan C, khong dung
 * phan cung -> test duoc tren PC (tests_host/modbus). */

#include <stdint.h>
#include "nanomodbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_SLAVE_DEMO_REG_COUNT		(3)

/* 0: phien ban app (major << 8 | minor), 1: patch, 2: uptime giay (u16).
 * App ghi vao, slave chi cho doc. */
extern uint16_t mb_slave_demo_regs[MB_SLAVE_DEMO_REG_COUNT];

extern void mb_slave_regs_bind(nmbs_callbacks* cb);

#ifdef __cplusplus
}
#endif

#endif /* __MB_SLAVE_REGS_H__ */
