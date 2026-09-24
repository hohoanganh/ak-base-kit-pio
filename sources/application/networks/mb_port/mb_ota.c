#include <string.h>
#include "mb_ota.h"

#if defined (TASK_MBSLAVE_EN)

static const mb_ota_ops_t* ota_ops;
static uint16_t ota_status;
static uint32_t ota_len;
static uint16_t ota_checksum;
static uint32_t ota_psk;
static uint32_t ota_received;
static uint32_t ota_last_offset;	/* offset khoi vua ghi, de nhan dien gui lai */
static uint16_t ota_last_len;

void mb_ota_init(const mb_ota_ops_t* ops) {
	ota_ops = ops;
	ota_status = MB_OTA_ST_IDLE;
	ota_len = ota_received = ota_psk = 0;
	ota_checksum = 0;
	ota_last_offset = 0xFFFFFFFFUL;
	ota_last_len = 0;
}

bool mb_ota_owns(uint16_t address, uint16_t quantity) {
	return address >= MB_OTA_REG_CMD && (uint32_t)address + quantity - 1 <= MB_OTA_REG_LAST;
}

static uint16_t reg_value(uint16_t address) {
	switch (address) {
	case MB_OTA_REG_STATUS:		return ota_status;
	case MB_OTA_REG_LEN_HI:		return (uint16_t)(ota_len >> 16);
	case MB_OTA_REG_LEN_HI + 1:	return (uint16_t)ota_len;
	case MB_OTA_REG_CHECKSUM:	return ota_checksum;
	case MB_OTA_REG_PSK_HI:		return (uint16_t)(ota_psk >> 16);
	case MB_OTA_REG_PSK_LO:		return (uint16_t)ota_psk;
	case MB_OTA_REG_RECV_HI:	return (uint16_t)(ota_received >> 16);
	case MB_OTA_REG_RECV_LO:	return (uint16_t)ota_received;
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
	if (ota_psk != MB_OTA_PSK || ota_len == 0) {
		ota_status = MB_OTA_ST_ERR_HEADER;
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	if (ota_len > MB_OTA_MAX_LEN) {
		ota_status = MB_OTA_ST_ERR_SIZE;
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	ota_ops->erase();		/* chan ~1-2 s: PC dat timeout rieng cho lenh nay */
	ota_received = 0;
	ota_last_offset = 0xFFFFFFFFUL;
	ota_last_len = 0;
	ota_status = MB_OTA_ST_RECEIVING;
	return NMBS_ERROR_NONE;
}

static nmbs_error cmd_commit(void) {
	if (ota_status != MB_OTA_ST_RECEIVING || ota_received < ota_len) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	/* flash sau bin_len da la 0xFF (vua xoa) -> tinh tron boi 4 giong PC dem 0xFF */
	uint32_t len4 = (ota_len + 3u) & ~3u;
	if (ota_ops->checksum(len4) != ota_checksum) {
		ota_status = MB_OTA_ST_ERR_CHECKSUM;
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	ota_status = MB_OTA_ST_COMMITTED;
	ota_ops->commit(ota_psk, ota_len, ota_checksum);
	return NMBS_ERROR_NONE;
}

static nmbs_error write_chunk(uint16_t quantity, const uint16_t* regs) {
	if (ota_status != MB_OTA_ST_RECEIVING) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	if (quantity < 3 || quantity > 2 + MB_OTA_CHUNK_MAX_REGS) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	uint32_t offset = ((uint32_t)regs[0] << 16) | regs[1];
	uint32_t n = (uint32_t)(quantity - 2) * 2;

	if (offset >= ota_len) {
		ota_status = MB_OTA_ST_ERR_SIZE;
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	}
	if (n > ota_len - offset) {
		n = ota_len - offset;	/* khoi cuoi: bo byte dem */
	}
	if (offset == ota_last_offset && offset + n == ota_received && n == ota_last_len) {
		return NMBS_ERROR_NONE;	/* PC gui lai vi mat phan hoi - da ghi roi */
	}
	if (offset != ota_received) {
		ota_status = MB_OTA_ST_ERR_OFFSET;
		return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
	}

	uint8_t buf[MB_OTA_CHUNK_MAX_REGS * 2];
	for (uint16_t i = 0; i < quantity - 2; i++) {
		buf[2 * i] = (uint8_t)(regs[2 + i] >> 8);
		buf[2 * i + 1] = (uint8_t)regs[2 + i];
	}
	ota_ops->write(offset, buf, (uint16_t)n);
	ota_last_offset = offset;
	ota_last_len = (uint16_t)n;
	ota_received += n;
	return NMBS_ERROR_NONE;
}

nmbs_error mb_ota_write(uint16_t address, uint16_t quantity, const uint16_t* regs) {
	if (address == MB_OTA_REG_CMD && quantity == 1) {
		switch (regs[0]) {
		case MB_OTA_CMD_BEGIN:	return cmd_begin();
		case MB_OTA_CMD_COMMIT:	return cmd_commit();
		case MB_OTA_CMD_ABORT:	mb_ota_init(ota_ops); return NMBS_ERROR_NONE;
		default:				return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
		}
	}
	if (address >= MB_OTA_REG_LEN_HI && (uint32_t)address + quantity - 1 <= MB_OTA_REG_PSK_LO) {
		if (ota_status == MB_OTA_ST_RECEIVING) {
			return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;	/* khong doi header giua chung */
		}
		for (uint16_t i = 0; i < quantity; i++) {
			uint16_t a = (uint16_t)(address + i), v = regs[i];
			if (a == MB_OTA_REG_LEN_HI)				ota_len = (ota_len & 0x0000FFFFUL) | ((uint32_t)v << 16);
			else if (a == MB_OTA_REG_LEN_HI + 1)	ota_len = (ota_len & 0xFFFF0000UL) | v;
			else if (a == MB_OTA_REG_CHECKSUM)		ota_checksum = v;
			else if (a == MB_OTA_REG_PSK_HI)		ota_psk = (ota_psk & 0x0000FFFFUL) | ((uint32_t)v << 16);
			else									ota_psk = (ota_psk & 0xFFFF0000UL) | v;
		}
		return NMBS_ERROR_NONE;
	}
	if (address == MB_OTA_REG_CHUNK) {
		return write_chunk(quantity, regs);
	}
	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

#endif /* TASK_MBSLAVE_EN */
