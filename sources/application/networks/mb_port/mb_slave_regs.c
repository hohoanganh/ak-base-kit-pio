#include "mb_slave_regs.h"
#include "mb_ota.h"

/* Chi bien dich khi bat slave: o che do master nanoMODBUS duoc build voi
 * NMBS_SERVER_DISABLED, struct nmbs_callbacks khong co cac truong ben duoi. */
#if defined (TASK_MBSLAVE_EN)

uint16_t mb_slave_demo_regs[MB_SLAVE_DEMO_REG_COUNT];

static nmbs_error cb_read_holding(uint16_t address, uint16_t quantity, uint16_t* registers_out,
								  uint8_t unit_id, void* arg) {
	(void)unit_id; (void)arg;

	if (mb_ota_owns(address, quantity)) {
		return mb_ota_read(address, quantity, registers_out);
	}
	if ((uint32_t)address + quantity > MB_SLAVE_DEMO_REG_COUNT) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}
	for (uint16_t i = 0; i < quantity; i++) {
		registers_out[i] = mb_slave_demo_regs[address + i];
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error cb_write_single(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
	(void)unit_id; (void)arg;
	if (mb_ota_owns(address, 1)) {
		return mb_ota_write(address, 1, &value);
	}
	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;	/* thanh ghi demo chi doc */
}

static nmbs_error cb_write_multi(uint16_t address, uint16_t quantity, const uint16_t* registers,
								 uint8_t unit_id, void* arg) {
	(void)unit_id; (void)arg;
	if (mb_ota_owns(address, quantity)) {
		return mb_ota_write(address, quantity, registers);
	}
	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

void mb_slave_regs_bind(nmbs_callbacks* cb) {
	cb->read_holding_registers = cb_read_holding;
	cb->write_single_register = cb_write_single;
	cb->write_multiple_registers = cb_write_multi;
}

#endif /* TASK_MBSLAVE_EN */
