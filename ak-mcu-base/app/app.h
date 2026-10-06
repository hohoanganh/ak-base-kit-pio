#ifndef __APP_H__
#define __APP_H__

#include <stdint.h>

#include "ak.h"
#include "fw_types.h"

#ifndef APP_VER_MAJOR
#define APP_VER_MAJOR		1
#define APP_VER_MINOR		1
#define APP_VER_PATCH		0
#endif

#ifndef APP_VER_BUILD
#define APP_VER_BUILD		0
#endif

#define APP_WDT_TIMEOUT_MS		(8000)
#define APP_HEARTBEAT_MS		(1000)

/* TASK_SYSTEM_ID */
enum {
	SYSTEM_SIG_INIT = AK_USER_DEFINE_SIG,
	SYSTEM_SIG_HEARTBEAT,
};

/* TASK_CONSOLE_ID */
enum {
	CONSOLE_SIG_INIT = AK_USER_DEFINE_SIG,
	CONSOLE_SIG_LINE,		/* common msg: one command line */
};

/* TASK_FW_ID */
enum {
	FW_SIG_INSTALL = AK_USER_DEFINE_SIG,	/* verified image ready in STAGING */
	FW_SIG_LOADER,							/* reset into bootloader */
	FW_SIG_RESET,
	FW_SIG_DO_RESET,						/* timer: reset once logs are flushed */
};

/* Running version: APP header in flash if valid, else the compiled version.
 * On MCU both match (header is linked into the .elf). */
extern const fw_version_t* app_version(void);

extern int app_main(void);

#endif /* __APP_H__ */
