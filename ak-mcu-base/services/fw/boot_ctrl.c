#include <string.h>

#include "boot_ctrl.h"
#include "crc.h"
#include "hal.h"

#define BOOT_CTRL_NVM_OFFSET	(0)

typedef char boot_ctrl_size_check[(sizeof(boot_ctrl_t) <= HAL_NVM_SIZE) ? 1 : -1];

static uint32_t boot_ctrl_crc(const boot_ctrl_t* ctrl) {
	return crc32_update(CRC32_INIT, ctrl, sizeof(boot_ctrl_t) - 4);
}

void boot_ctrl_load(boot_ctrl_t* ctrl) {
	if (hal_nvm_read(BOOT_CTRL_NVM_OFFSET, ctrl, sizeof(boot_ctrl_t)) != 0 ||
			ctrl->magic != BOOT_CTRL_MAGIC ||
			ctrl->crc32 != boot_ctrl_crc(ctrl)) {
		memset(ctrl, 0, sizeof(boot_ctrl_t));
		ctrl->magic = BOOT_CTRL_MAGIC;
		ctrl->cmd = BOOT_CMD_NONE;
	}
}

int boot_ctrl_save(boot_ctrl_t* ctrl) {
	boot_ctrl_t verify;

	ctrl->magic = BOOT_CTRL_MAGIC;
	ctrl->crc32 = boot_ctrl_crc(ctrl);

	if (hal_nvm_write(BOOT_CTRL_NVM_OFFSET, ctrl, sizeof(boot_ctrl_t)) != 0) {
		return -1;
	}

	/* read back to confirm the NVM write */
	if (hal_nvm_read(BOOT_CTRL_NVM_OFFSET, &verify, sizeof(verify)) != 0 ||
			memcmp(&verify, ctrl, sizeof(verify)) != 0) {
		return -2;
	}

	return 0;
}

int boot_ctrl_set_cmd(uint8_t cmd) {
	boot_ctrl_t ctrl;

	boot_ctrl_load(&ctrl);
	if (ctrl.cmd == cmd) {
		return 0;
	}
	ctrl.cmd = cmd;
	return boot_ctrl_save(&ctrl);
}
