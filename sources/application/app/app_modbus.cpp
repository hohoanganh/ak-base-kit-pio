#include "app_modbus.h"

#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)

#include "app.h"
#include "rs485_port.h"
#include "sys_ctrl.h"
#if defined (TASK_MBSLAVE_EN)
#include "mb_slave_regs.h"
#endif

nmbs_t app_mb;

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
