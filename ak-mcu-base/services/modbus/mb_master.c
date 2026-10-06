/* master role: left out of a slave-only build (-DNMBS_CLIENT_DISABLED) */
#if !defined(NMBS_CLIENT_DISABLED)

#include "mb_master.h"
#include "mb_port.h"
#include "hal_rs485.h"

#define MB_MASTER_BYTE_TIMEOUT_MS	(20)

static nmbs_t client;
static uint8_t ready;

int mb_master_init(uint32_t baudrate, uint16_t response_timeout_ms) {
	nmbs_platform_conf conf;

	hal_rs485_init(baudrate);
	mb_port_conf(&conf);

	if (nmbs_client_create(&client, &conf) != NMBS_ERROR_NONE) {
		return MB_ERR_ARG;
	}
	nmbs_set_read_timeout(&client, response_timeout_ms);
	nmbs_set_byte_timeout(&client, MB_MASTER_BYTE_TIMEOUT_MS);
	ready = 1;
	return MB_OK;
}

/* Bytes of an earlier, late answer must not be taken for this one. */
static uint8_t prepare(uint8_t unit_id) {
	if (!ready) {
		return 0;
	}
	while (hal_rs485_getc() >= 0) {
	}
	nmbs_set_destination_rtu_address(&client, unit_id);
	return 1;
}

int mb_master_read_holding(uint8_t unit_id, uint16_t address, uint16_t quantity, uint16_t* out) {
	return prepare(unit_id) ? (int)nmbs_read_holding_registers(&client, address, quantity, out) : MB_ERR_ARG;
}

int mb_master_read_input(uint8_t unit_id, uint16_t address, uint16_t quantity, uint16_t* out) {
	return prepare(unit_id) ? (int)nmbs_read_input_registers(&client, address, quantity, out) : MB_ERR_ARG;
}

int mb_master_write_single(uint8_t unit_id, uint16_t address, uint16_t value) {
	return prepare(unit_id) ? (int)nmbs_write_single_register(&client, address, value) : MB_ERR_ARG;
}

int mb_master_write_multiple(uint8_t unit_id, uint16_t address, uint16_t quantity, const uint16_t* regs) {
	return prepare(unit_id) ? (int)nmbs_write_multiple_registers(&client, address, quantity, regs) : MB_ERR_ARG;
}

const char* mb_err_str(int err) {
	switch (err) {
	case MB_OK:					return "ok";
	case 1:						return "exception 1: illegal function";
	case 2:						return "exception 2: illegal data address";
	case 3:						return "exception 3: illegal data value";
	case 4:						return "exception 4: device failure";
	case MB_ERR_ARG:			return "bad argument";
	case MB_ERR_BAD_RESPONSE:	return "bad response";
	case MB_ERR_TIMEOUT:		return "timeout";
	case MB_ERR_TRANSPORT:		return "transport error";
	case MB_ERR_CRC:			return "crc error";
	case MB_ERR_UNIT_ID:		return "answer from another unit";
	default:					return "error";
	}
}

#endif /* !NMBS_CLIENT_DISABLED */
