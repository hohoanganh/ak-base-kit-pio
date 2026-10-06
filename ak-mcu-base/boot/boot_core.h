/**
 ******************************************************************************
 * @brief:  Chip-independent bootloader logic (unit tested on host).
 *
 *  boot_decide()  : pick an action from the current state (pure, no I/O)
 *  boot_install() : copy STAGING -> APP. The header page is written LAST, so
 *                   after a power loss APP is always invalid and the next
 *                   boot copies again from the intact STAGING.
 ******************************************************************************
**/

#ifndef __BOOT_CORE_H__
#define __BOOT_CORE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "fw_types.h"
#include "fw_image.h"

#ifndef BOOT_MAX_INSTALL_ATTEMPTS
#define BOOT_MAX_INSTALL_ATTEMPTS	(3)
#endif

typedef enum {
	BOOT_ACT_RUN = 0,		/* APP valid */
	BOOT_ACT_RUN_RAW,		/* APP has no header but a sane vector table (debug SWD flash),
							 * only when no install is pending */
	BOOT_ACT_INSTALL,		/* install image from STAGING */
	BOOT_ACT_LOADER,		/* stay in bootloader, wait for UART download */
} boot_act_t;

typedef struct {
	uint8_t cmd;				/* boot_ctrl.cmd */
	uint8_t install_attempts;	/* boot_ctrl.install_attempts */
	uint8_t app_ok;				/* fw_image_verify(APP) == FW_OK */
	uint8_t app_raw_ok;			/* APP has no magic but a sane vector table */
	uint8_t staging_ok;			/* fw_image_verify(STAGING) == FW_OK */
} boot_state_t;

extern boot_act_t boot_decide(const boot_state_t* s);
extern const char* boot_act_str(boot_act_t act);

/* Copy the (verified) image from STAGING to APP, then verify APP. */
extern fw_err_t boot_install(const fw_image_hdr_t* staging_hdr);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_CORE_H__ */
