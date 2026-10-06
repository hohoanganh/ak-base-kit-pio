/* STM32L151 port internals (not used outside port/). */
#ifndef __PORT_STM32_H__
#define __PORT_STM32_H__

#include <stdint.h>

#define PORT_JUMP_MAGIC			(0x4A4D5021UL)	/* boot -> app */
#define PORT_REASON_MAGIC		(0x52535421UL)	/* reset_reason valid */
#define PORT_FAULT_MAGIC		(0x46415521UL)	/* previous run hit a HardFault */

/* .noinit RAM (start of SRAM, same address in boot and app): survives soft reset. */
typedef struct {
	uint32_t jump_magic;
	uint32_t jump_addr;
	uint32_t reason_magic;
	uint32_t reset_reason;
	uint32_t fault_magic;
	uint32_t fault_pc;
	uint32_t fault_lr;
	uint32_t fault_cfsr;
} port_noinit_t;

extern port_noinit_t port_noinit;

#endif
