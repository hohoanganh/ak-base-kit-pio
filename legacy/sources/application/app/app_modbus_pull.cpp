/* Client doc thiet bi Modbus qua nanoMODBUS (app_mb) - chi co nghia khi
 * TASK_MBMASTER_EN bat. Boc ca file de tat duoc co do.
 * Xem docs/known-bugs.md #6. */
#if defined (TASK_MBMASTER_EN)

/* kernel include */
#include "ak.h"
#include "message.h"
#include "timer.h"
#include "fsm.h"

/* driver include */
#include "led.h"
#include "button.h"
#include "flash.h"

/* app include */
#include "app.h"
#include "app_dbg.h"
#include "app_bsp.h"
#include "app_flash.h"
#include "app_data.h"
#include "app_non_clear_ram.h"
#include "app_modbus_pull.h"
#include "app_modbus.h"

/* sys include */
#include "sys_io.h"
#include "sys_ctrl.h"


/*----------------------------------------------------------------------------*
 *  Device name: Cảm biến nhiệt độ, độ ẩm RS485 Modbus RTU (SHT35)
 *  Note:
 *----------------------------------------------------------------------------*/
static MB_DevRegStruct_t ThSensor_Registers[] = {
	{ 0  , REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_REGISTERS, 0.1, (const int8_t*)"*C", INT16U10, true , (const int8_t*)"Temperature value"},
	{ 1  , REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_REGISTERS, 0.1, (const int8_t*)"RH", INT16U10, true , (const int8_t*)"Humidity value"   },
	{ 100, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_REGISTERS,   1, (const int8_t*)"NA", INT16U10, false, (const int8_t*)"Device address"   },
	{ 101, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_REGISTERS,   1, (const int8_t*)"NA", INT16U10, false, (const int8_t*)"Device baudrate"  },
};

MB_DeviceStruct_t MB_TH_Sensor = {
	.tId = 2,
	.tBaud = 9600,
	.listRegDevice = ThSensor_Registers,
	.listRegAmount = sizeof(ThSensor_Registers) / sizeof(ThSensor_Registers[0])
};

/*----------------------------------------------------------------------------*
 *  Device name: Relay 4 kênh IO giao tiếp RS485/RS232 công nghiệp LH-IO-01
 *  Note:
 *----------------------------------------------------------------------------*/
static MB_DevRegStruct_t MB_LHIO404_IO_Device_Registers[] = {
	{ 1, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_COILS         , 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Ouput 2 status"},
	{ 2, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_COILS         , 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Ouput 3 status"},
	{ 0, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_COILS         , 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Ouput 1 status"},
	{ 3, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_COILS         , 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Ouput 4 status"},
	{ 0, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_DISCRETE_INPUT, 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Input 1 status"},
	{ 1, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_DISCRETE_INPUT, 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Input 2 status"},
	{ 2, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_DISCRETE_INPUT, 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Input 3 status"},
	{ 3, REG_VAL_DEFAULT, MODBUS_FUNCTION_READ_DISCRETE_INPUT, 1, (const int8_t*)"NA", INT8U, true, (const int8_t*)"Input 4 status"},
};

MB_DeviceStruct_t MB_LHIO404_IO_Device = {
	.tId = 3,
	.tBaud = 9600,
	.listRegDevice = MB_LHIO404_IO_Device_Registers,
	.listRegAmount = sizeof(MB_LHIO404_IO_Device_Registers) / sizeof(MB_LHIO404_IO_Device_Registers[0])
};

/* Private functions prototypes -----------------------------------------------*/
static nmbs_error appMBMasterRead(uint8_t slAddr, uint8_t funCode, uint16_t addReg, uint16_t *buf);

/* Function implementation ---------------------------------------------------*/
void updateDataModbusDevice(MB_DeviceStruct_t *mbDevice) {
	uint8_t retryCount = 0;
	uint16_t regVal;
	uint16_t regAddr;
	uint8_t funCode;

	for (uint16_t regIndex = 0; regIndex < mbDevice->listRegAmount; ++regIndex) {
		funCode = mbDevice->listRegDevice[regIndex].funcCode;
		regAddr = mbDevice->listRegDevice[regIndex].regAddress;
		regVal = 0;

		nmbs_error errCode = appMBMasterRead(mbDevice->tId, funCode, regAddr, &regVal);

		if (errCode != NMBS_ERROR_NONE) {
			mbDevice->listRegDevice[regIndex].regValue = REG_VAL_DEFAULT;
			++retryCount;
		}
		else {
			mbDevice->listRegDevice[regIndex].regValue = regVal;
		}

		if (retryCount > MB_READ_FAILED_RETRY_MAX) {
			break;
		}
	}
}

nmbs_error appMBMasterRead(uint8_t slAddr, uint8_t funCode, uint16_t addReg, uint16_t *buf) {
	nmbs_set_destination_rtu_address(&app_mb, slAddr);

	switch (funCode) {
	case MODBUS_FUNCTION_READ_COILS:
	case MODBUS_FUNCTION_READ_DISCRETE_INPUT: {
		nmbs_bitfield bits;
		nmbs_error err = (funCode == MODBUS_FUNCTION_READ_COILS)
				? nmbs_read_coils(&app_mb, addReg, 1, bits)
				: nmbs_read_discrete_inputs(&app_mb, addReg, 1, bits);
		if (err == NMBS_ERROR_NONE) {
			*buf = nmbs_bitfield_read(bits, 0) ? 1 : 0;
		}
		return err;
	}

	case MODBUS_FUNCTION_READ_REGISTERS:
		return nmbs_read_holding_registers(&app_mb, addReg, 1, buf);

	case MODBUS_FUNCTION_READ_INPUT_REGISTER:
		return nmbs_read_input_registers(&app_mb, addReg, 1, buf);

	default:
		return NMBS_ERROR_INVALID_ARGUMENT;
	}
}

#endif /* TASK_MBMASTER_EN */
