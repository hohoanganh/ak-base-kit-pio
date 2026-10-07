#include <stdio.h>
#include "fake_link.h"
#include "mb_slave_regs.h"

static nmbs_t client, server;

static void setup(void) {
	nmbs_callbacks cb;
	fake_link_reset();
	nmbs_callbacks_create(&cb);
	mb_slave_regs_bind(&cb);
	fake_make_server(&server, 1, &cb);
	fake_make_client(&client, 1);
	mb_slave_demo_regs[0] = 0x0102; mb_slave_demo_regs[1] = 3; mb_slave_demo_regs[2] = 42;
}

int main(void) {
	uint16_t r[3] = {0};

	setup();	/* doc du 3 thanh ghi demo */
	CHECK(nmbs_read_holding_registers(&client, 0, 3, r) == NMBS_ERROR_NONE);
	CHECK(r[0] == 0x0102 && r[1] == 3 && r[2] == 42);

	setup();	/* vuot bang -> exception 2 */
	CHECK(nmbs_read_holding_registers(&client, 2, 2, r) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

	setup();	/* thanh ghi demo chi doc */
	CHECK(nmbs_write_single_register(&client, 0, 7) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

	setup();	/* phan hoi sai CRC */
	fake_link_corrupt_next_response();
	CHECK(nmbs_read_holding_registers(&client, 0, 1, r) == NMBS_ERROR_CRC);

	setup();	/* hoi nham dia chi slave -> server im, client het gio */
	nmbs_set_destination_rtu_address(&client, 5);
	CHECK(nmbs_read_holding_registers(&client, 0, 1, r) == NMBS_ERROR_TIMEOUT);

	printf("test_slave_regs: %s\n", fake_failures ? "FAIL" : "OK");
	return fake_failures ? 1 : 0;
}
