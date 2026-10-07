/* slave role: left out of a master-only build (-DNMBS_SERVER_DISABLED) */
#if !defined(NMBS_SERVER_DISABLED)

#include <string.h>

#include "mb_ota.h"
#include "fw_update.h"
#include "fw_image.h"
#include "hal_flash.h"

static void (*ota_on_commit)(void);
static uint16_t ota_status;
static uint32_t ota_len;
static uint16_t ota_checksum;		/* kept for register compatibility, not used */
static uint32_t ota_psk;
static uint16_t ota_detail;			/* fw_err_t of the last failure */
static uint32_t ota_last_offset;	/* chunk written last, to recognise a resend */
static uint16_t ota_last_len;

void mb_ota_init(void (*on_commit)(void)) {
	ota_on_commit = on_commit;
	ota_status = MB_OTA_ST_IDLE;
	ota_len = ota_psk = 0;
	ota_checksum = 0;
	ota_detail = 0;
	ota_last_offset = 0xFFFFFFFFUL;
	ota_last_len = 0;
	fw_update_abort();
}

uint8_t mb_ota_owns(uint16_t address, uint16_t quantity) {
	return address >= MB_OTA_REG_CMD && (uint32_t)address + quantity - 1 <= MB_OTA_REG_LAST;
}

static uint16_t status_of(fw_err_t err) {
	switch (err) {
	case FW_ERR_IMG_CRC:	return MB_OTA_ST_ERR_CHECKSUM;
	case FW_ERR_TOO_BIG:
	case FW_ERR_SIZE:		return MB_OTA_ST_ERR_SIZE;
	case FW_ERR_OFFSET:		return MB_OTA_ST_ERR_OFFSET;
	default:				return MB_OTA_ST_ERR_HEADER;	/* magic, header crc, board, address, vector, flash */
	}
}

static nmbs_error fail(fw_err_t err, nmbs_error exception) {
	ota_detail = (uint16_t)err;
	ota_status = status_of(err);
	fw_update_abort();
	return exception;
}

static uint16_t reg_value(uint16_t address) {
	uint32_t received = (ota_status == MB_OTA_ST_IDLE) ? 0 : fw_update_written();

	switch (address) {
	case MB_OTA_REG_STATUS:		return ota_status;
	case MB_OTA_REG_LEN_HI:		return (uint16_t)(ota_len >> 16);
	case MB_OTA_REG_LEN_LO:		return (uint16_t)ota_len;
	case MB_OTA_REG_CHECKSUM:	return ota_checksum;
	case MB_OTA_REG_PSK_HI:		return (uint16_t)(ota_psk >> 16);
	case MB_OTA_REG_PSK_LO:		return (uint16_t)ota_psk;
	case MB_OTA_REG_RECV_HI:	return (uint16_t)(received >> 16);
	case MB_OTA_REG_RECV_LO:	return (uint16_t)received;
	case MB_OTA_REG_DETAIL:		return ota_detail;
	default:					return 0;
	}
}

nmbs_error mb_ota_read(uint16_t address, uint16_t quantity, uint16_t* out) {
	for (uint16_t i = 0; i < quantity; i++) {
		out[i] = reg_value((uint16_t)(address + i));
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error cmd_begin(void) {
	fw_err_t err;

	if (!ota_on_commit) {
		return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
	}
	if (ota_psk != MB_OTA_PSK_IMG) {
		return fail(FW_ERR_MAGIC, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}
	/* pages are erased as the data arrives: BEGIN answers at once */
	err = fw_update_begin(ota_len);
	if (err != FW_OK) {
		return fail(err, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}
	ota_detail = 0;
	ota_last_offset = 0xFFFFFFFFUL;
	ota_last_len = 0;
	ota_status = MB_OTA_ST_RECEIVING;
	return NMBS_ERROR_NONE;
}

static nmbs_error cmd_commit(void) {
	fw_image_hdr_t hdr;
	fw_err_t err;

	if (ota_status == MB_OTA_ST_COMMITTED) {
		return NMBS_ERROR_NONE;		/* COMMIT sent again: already done, do not commit twice */
	}
	if (!ota_on_commit) {
		return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
	}
	if (ota_status != MB_OTA_ST_RECEIVING || fw_update_written() < ota_len) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	err = fw_update_finish(&hdr);
	if (err != FW_OK) {
		return fail(err, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}
	ota_status = MB_OTA_ST_COMMITTED;
	ota_on_commit();
	return NMBS_ERROR_NONE;
}

static nmbs_error write_chunk(uint16_t quantity, const uint16_t* regs) {
	uint8_t buf[MB_OTA_CHUNK_MAX_REGS * 2];
	uint32_t offset;
	uint32_t n;
	fw_err_t err;

	if (!ota_on_commit) {
		return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
	}
	if (ota_status != MB_OTA_ST_RECEIVING) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	if (quantity < 3 || quantity > 2 + MB_OTA_CHUNK_MAX_REGS) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	offset = ((uint32_t)regs[0] << 16) | regs[1];
	n = (uint32_t)(quantity - 2) * 2;

	if (offset >= ota_len) {
		return fail(FW_ERR_OFFSET, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
	}
	if (n > ota_len - offset) {
		n = ota_len - offset;		/* last chunk: an odd image size ends inside a register */
	}
	if (offset == ota_last_offset && n == ota_last_len && offset + n == fw_update_written()) {
		return NMBS_ERROR_NONE;		/* sent again because the answer was lost: already written */
	}
	if (offset != fw_update_written()) {
		return fail(FW_ERR_OFFSET, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
	}

	for (uint16_t i = 0; i < quantity - 2; i++) {
		buf[2 * i] = (uint8_t)(regs[2 + i] >> 8);
		buf[2 * i + 1] = (uint8_t)regs[2 + i];
	}
	err = fw_update_write(offset, buf, n);
	if (err != FW_OK) {
		return fail(err, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
	}
	ota_last_offset = offset;
	ota_last_len = (uint16_t)n;
	return NMBS_ERROR_NONE;
}

nmbs_error mb_ota_write(uint16_t address, uint16_t quantity, const uint16_t* regs) {
	if (address == MB_OTA_REG_CMD && quantity == 1) {
		switch (regs[0]) {
		case MB_OTA_CMD_BEGIN:
			if (ota_status == MB_OTA_ST_COMMITTED) {
				return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;	/* waiting for the reset: no new session */
			}
			return cmd_begin();

		case MB_OTA_CMD_COMMIT:
			return cmd_commit();

		case MB_OTA_CMD_ABORT:
			if (ota_status == MB_OTA_ST_COMMITTED) {
				return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
			mb_ota_init(ota_on_commit);
			return NMBS_ERROR_NONE;

		default:
			return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
		}
	}

	if (address >= MB_OTA_REG_LEN_HI && (uint32_t)address + quantity - 1 <= MB_OTA_REG_PSK_LO) {
		if (ota_status == MB_OTA_ST_RECEIVING || ota_status == MB_OTA_ST_COMMITTED) {
			return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;	/* no header change during or after a session */
		}
		for (uint16_t i = 0; i < quantity; i++) {
			uint16_t a = (uint16_t)(address + i);
			uint16_t v = regs[i];

			if (a == MB_OTA_REG_LEN_HI) {
				ota_len = (ota_len & 0x0000FFFFUL) | ((uint32_t)v << 16);
			}
			else if (a == MB_OTA_REG_LEN_LO) {
				ota_len = (ota_len & 0xFFFF0000UL) | v;
			}
			else if (a == MB_OTA_REG_CHECKSUM) {
				ota_checksum = v;
			}
			else if (a == MB_OTA_REG_PSK_HI) {
				ota_psk = (ota_psk & 0x0000FFFFUL) | ((uint32_t)v << 16);
			}
			else {
				ota_psk = (ota_psk & 0xFFFF0000UL) | v;
			}
		}
		return NMBS_ERROR_NONE;
	}

	if (address == MB_OTA_REG_CHUNK) {
		return write_chunk(quantity, regs);
	}

	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

#endif /* !NMBS_SERVER_DISABLED */
