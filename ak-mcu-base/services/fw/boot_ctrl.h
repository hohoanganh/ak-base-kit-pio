/**
 ******************************************************************************
 * @brief:  Boot <-> app shared control block (in NVM, survives reset).
 *
 *  App update: write image to STAGING, verify, set cmd = BOOT_CMD_UPDATE,
 *  reset. Boot sees UPDATE -> verify STAGING -> copy to APP -> verify ->
 *  clear cmd -> run app. Power loss midway: cmd is still set and STAGING is
 *  intact -> next boot copies again. NVM is NOT written on every boot
 *  (endurance).
 ******************************************************************************
**/

#ifndef __BOOT_CTRL_H__
#define __BOOT_CTRL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define BOOT_CTRL_MAGIC			(0x4C54434BUL)	/* "KCTL" */

#define BOOT_CMD_NONE			(0)
#define BOOT_CMD_UPDATE			(1)	/* install image from STAGING, then run */
#define BOOT_CMD_LOADER			(2)	/* stay in bootloader, wait for UART download */

typedef struct {
	uint32_t magic;
	uint8_t  cmd;
	uint8_t  install_attempts;	/* consecutive unfinished install attempts */
	uint8_t  last_result;		/* fw_err_t of the last install */
	uint8_t  reserved;
	uint32_t install_count;		/* total successful installs */
	uint32_t crc32;
} boot_ctrl_t;

/* Load; blank/corrupt NVM yields defaults (cmd NONE). */
extern void boot_ctrl_load(boot_ctrl_t* ctrl);
/* Update crc and write. Returns 0 on success. */
extern int boot_ctrl_save(boot_ctrl_t* ctrl);
/* Helper for the app: load, change cmd, save. */
extern int boot_ctrl_set_cmd(uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_CTRL_H__ */
