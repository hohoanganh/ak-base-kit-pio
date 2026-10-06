/* Application, STM32L151CB: APP partition 58K @ 0x08003000.
 * [0x08003000] .fw_header (256 B image header, patched by tools/mkimage.py)
 * [0x08003100] vector table
 * Keep in sync with port_cfg.h. */
ENTRY(reset_handler)

MEMORY {
	FLASH (rx)	: ORIGIN = 0x08003000, LENGTH = 58K
	RAM (rwx)	: ORIGIN = 0x20000000, LENGTH = 16K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_min_stack = 2K;

SECTIONS {
	.fw_header : {
		KEEP(*(.fw_header))
	} > FLASH

	INCLUDE sections.ld
}

ASSERT(ADDR(.fw_header) == ORIGIN(FLASH), ".fw_header must be first in APP")
ASSERT(SIZEOF(.fw_header) == 256, ".fw_header must be 256 bytes")
ASSERT(_isr_vector == ORIGIN(FLASH) + 256, "vector table must follow the header")
ASSERT(ADDR(.noinit) == ORIGIN(RAM), ".noinit must start at RAM origin (shared with boot)")
