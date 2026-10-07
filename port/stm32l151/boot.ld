/* Bootloader, STM32L151CB: 12K @ 0x08000000. Keep in sync with port_cfg.h. */
ENTRY(reset_handler)

MEMORY {
	FLASH (rx)	: ORIGIN = 0x08000000, LENGTH = 12K
	RAM (rwx)	: ORIGIN = 0x20000000, LENGTH = 16K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_min_stack = 2K;

SECTIONS {
	INCLUDE sections.ld
}

ASSERT(ADDR(.noinit) == ORIGIN(RAM), ".noinit must start at RAM origin (shared with app)")
