#include "app_modbus.h"

#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)

#include "app.h"
#include "rs485_port.h"
#include "sys_ctrl.h"
#if defined (TASK_MBSLAVE_EN)
#include "mb_slave_regs.h"
#include "mb_ota.h"
#include "task_fw.h"
#include "sys_boot.h"
#endif

nmbs_t app_mb;

#if defined (TASK_MBSLAVE_EN)
static void ota_commit(uint32_t psk, uint32_t bin_len, uint16_t checksum) {
	firmware_header_t header;
	header.psk = psk;
	header.bin_len = bin_len;
	header.checksum = checksum;
	fw_commit_app_later(&header);
}

static const mb_ota_ops_t ota_ops = {
	fw_ext_erase, fw_ext_write, fw_ext_checksum, ota_commit
};

static_assert(MB_OTA_PSK == FIRMWARE_PSK, "MB_OTA_PSK phai bang FIRMWARE_PSK");
#endif

nmbs_error app_modbus_init(void) {
	nmbs_platform_conf conf;
	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = rs485_port_read;
	conf.write = rs485_port_write;

	rs485_port_init(APP_MB_BAUDRATE);

#if defined (TASK_MBMASTER_EN)
	nmbs_error err = nmbs_client_create(&app_mb, &conf);
	nmbs_set_read_timeout(&app_mb, APP_MB_READ_TIMEOUT_MS);
#else
	mb_ota_init(&ota_ops);
	nmbs_callbacks cb;
	nmbs_callbacks_create(&cb);
	mb_slave_regs_bind(&cb);
#if defined(APP_VER_MAJOR) && defined(APP_VER_MINOR) && defined(APP_VER_PATCH)
	mb_slave_demo_regs[0] = (uint16_t)((APP_VER_MAJOR << 8) | APP_VER_MINOR);
	mb_slave_demo_regs[1] = (uint16_t)APP_VER_PATCH;
#else
	mb_slave_demo_regs[0] = 0;
	mb_slave_demo_regs[1] = 0;
#endif
	nmbs_error err = nmbs_server_create(&app_mb, APP_MB_SLAVE_ADDR, &conf, &cb);
	nmbs_set_read_timeout(&app_mb, 0);	/* poll khong chan khi duong truyen ranh */
#endif
	nmbs_set_byte_timeout(&app_mb, APP_MB_BYTE_TIMEOUT_MS);
	return err;
}

void app_modbus_poll(void) {
#if defined (TASK_MBSLAVE_EN)
	mb_slave_demo_regs[2] = (uint16_t)(sys_ctrl_millis() / 1000);
	nmbs_server_poll(&app_mb);
#endif
}

#endif /* TASK_MBMASTER_EN || TASK_MBSLAVE_EN */
