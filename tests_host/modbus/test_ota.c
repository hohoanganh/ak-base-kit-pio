#include <stdio.h>
#include <string.h>
#include "fake_link.h"
#include "mb_slave_regs.h"
#include "mb_ota.h"

/* external flash gia */
static uint8_t flash[MB_OTA_MAX_LEN + 8];
static int erased, committed, writes;
static uint32_t commit_len;

static void f_erase(void) { memset(flash, 0xFF, sizeof flash); erased++; }
static void f_write(uint32_t off, const uint8_t* d, uint16_t n) { memcpy(flash + off, d, n); writes++; }
static uint16_t f_checksum(uint32_t len) {	/* cung thuat toan fw_ext_checksum */
	uint32_t s = 0;
	for (uint32_t i = 0; i < len; i += 4) {
		uint32_t w; memcpy(&w, flash + i, 4); s += w;	/* PC x86 cung little-endian nhu Cortex-M */
	}
	return (uint16_t)s;
}
static void f_commit(uint32_t psk, uint32_t len, uint16_t cs) { (void)psk; (void)cs; committed++; commit_len = len; }
static const mb_ota_ops_t ops = { f_erase, f_write, f_checksum, f_commit };

static nmbs_t client, server;
static uint8_t image[1024];	/* du cho ca truong hop dai 1004 */

static uint16_t img_checksum(const uint8_t* d, uint32_t n) {	/* anh da la boi 4 */
	uint32_t s = 0;
	for (uint32_t i = 0; i < n; i += 4) {
		s += (uint32_t)d[i] | (uint32_t)d[i+1] << 8 | (uint32_t)d[i+2] << 16 | (uint32_t)d[i+3] << 24;
	}
	return (uint16_t)s;
}

static void setup(void) {
	nmbs_callbacks cb;
	fake_link_reset();
	nmbs_callbacks_create(&cb);
	mb_slave_regs_bind(&cb);
	mb_ota_init(&ops);
	fake_make_server(&server, 1, &cb);
	fake_make_client(&client, 1);
	erased = committed = writes = 0;
	for (unsigned i = 0; i < sizeof image; i++) image[i] = (uint8_t)(i * 7 + 3);
}

static nmbs_error write_header(uint32_t len, uint16_t cs, uint32_t psk) {
	uint16_t h[5] = { (uint16_t)(len >> 16), (uint16_t)len, cs, (uint16_t)(psk >> 16), (uint16_t)psk };
	return nmbs_write_multiple_registers(&client, MB_OTA_REG_LEN_HI, 5, h);
}

static nmbs_error send_chunk(uint32_t off, const uint8_t* d, uint16_t n) {
	uint16_t r[2 + MB_OTA_CHUNK_MAX_REGS];
	uint16_t nregs = (uint16_t)((n + 1) / 2);
	r[0] = (uint16_t)(off >> 16); r[1] = (uint16_t)off;
	for (uint16_t i = 0; i < nregs; i++) {
		uint8_t hi = d[2*i], lo = (2*i + 1 < n) ? d[2*i + 1] : 0xFF;
		r[2 + i] = (uint16_t)((uint16_t)hi << 8 | lo);
	}
	return nmbs_write_multiple_registers(&client, MB_OTA_REG_CHUNK, (uint16_t)(2 + nregs), r);
}

static uint16_t status(void) { uint16_t s = 0xDEAD; nmbs_read_holding_registers(&client, MB_OTA_REG_STATUS, 1, &s); return s; }

static void send_all(uint32_t n) {
	for (uint32_t off = 0; off < n; off += 128) {
		uint16_t k = (uint16_t)((n - off) < 128 ? (n - off) : 128);
		CHECK(send_chunk(off, image + off, k) == NMBS_ERROR_NONE);
	}
}

int main(void) {
	uint32_t n = 1000;
	uint16_t cs;
	setvbuf(stdout, NULL, _IONBF, 0);	/* thay doi de khong mat log CHECK khi crash */

	/* 1. Du luong thanh cong */
	setup(); cs = img_checksum(image, n);
	CHECK(write_header(n, cs, MB_OTA_PSK) == NMBS_ERROR_NONE);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_ERROR_NONE);
	CHECK(erased == 1 && status() == MB_OTA_ST_RECEIVING);
	send_all(n);
	{ uint16_t rv[2]; CHECK(nmbs_read_holding_registers(&client, MB_OTA_REG_RECV_HI, 2, rv) == NMBS_ERROR_NONE);
	  CHECK(((uint32_t)rv[0] << 16 | rv[1]) == n); }
	CHECK(memcmp(flash, image, n) == 0);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_ERROR_NONE);
	CHECK(committed == 1 && commit_len == n && status() == MB_OTA_ST_COMMITTED);

	/* 2. Gui lai khoi vua gui (mat phan hoi) -> OK, khong ghi lai flash; nhay coc -> exception 4 */
	setup(); cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	CHECK(send_chunk(0, image, 128) == NMBS_ERROR_NONE);
	CHECK(writes == 1);
	CHECK(send_chunk(0, image, 128) == NMBS_ERROR_NONE);			/* gui lai */
	CHECK(writes == 1);						/* khong ghi lai flash lan 2 */
	CHECK(send_chunk(512, image + 512, 128) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
	CHECK(status() == MB_OTA_ST_ERR_OFFSET);

	/* 3. Sai checksum -> exception 3, STATUS 0x8003, khong commit */
	setup(); cs = img_checksum(image, n);
	write_header(n, (uint16_t)(cs + 1), MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	send_all(n);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(committed == 0 && status() == MB_OTA_ST_ERR_CHECKSUM);

	/* 4. Sai psk / qua co -> BEGIN bi tu choi, khong xoa flash */
	setup(); cs = img_checksum(image, n);
	write_header(n, cs, 0x11111111UL);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(erased == 0 && status() == MB_OTA_ST_ERR_HEADER);
	write_header(MB_OTA_MAX_LEN + 4, cs, MB_OTA_PSK);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(erased == 0 && status() == MB_OTA_ST_ERR_SIZE);

	/* 5. COMMIT khi chua nhan du -> tu choi; ABORT ve IDLE; khoi khi chua BEGIN -> tu choi */
	setup(); cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	send_chunk(0, image, 128);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(committed == 0);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_ABORT) == NMBS_ERROR_NONE);
	CHECK(status() == MB_OTA_ST_IDLE);
	CHECK(send_chunk(0, image, 128) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

	/* 6. bin_len khong boi 4 (1003) -> BEGIN bi tu choi ngay, STATUS 0x8001, khong xoa flash.
	 * PC luon dem anh bang 0xFF cho du boi 4 truoc khi gui, nen do dai le nhu the nay
	 * khong con hop le nua. */
	setup(); n = 1003; cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(erased == 0 && status() == MB_OTA_ST_ERR_HEADER);

	/* 6b. Anh boi 4 nhung khong boi 128 (khoi cuoi ngan hon 128 byte): byte cuoi cua anh
	 * la 0xFF ma PC da dem vao cho du boi 4 -> COMMIT thanh cong, flash tu bin_len tro di
	 * van la 0xFF (vua xoa, khong bi khoi nao ghi de) */
	setup(); n = 1004; image[1003] = 0xFF;
	cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	send_all(n);
	CHECK(memcmp(flash, image, n) == 0);
	CHECK(flash[n] == 0xFF);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_ERROR_NONE);
	CHECK(committed == 1 && commit_len == n);

	/* 7. Vector checksum dung chung voi Python: byte 0..9 dem 0xFF -> 0x0F0C */
	{ uint8_t v[12] = {0,1,2,3,4,5,6,7,8,9,0xFF,0xFF}; CHECK(img_checksum(v, 12) == 0x0F0C); }

	/* 8. Thanh ghi demo van doc duoc ben canh vung OTA */
	{ uint16_t r; CHECK(nmbs_read_holding_registers(&client, 0, 1, &r) == NMBS_ERROR_NONE); }

	/* 9. Sau khi COMMIT thanh cong: BEGIN/ABORT/ghi header deu bi tu choi, STATUS giu nguyen 2,
	 * khong xoa flash them lan nao (dang cho reset tu bo boot-share-flash) */
	setup(); n = 1000; cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	send_all(n);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_ERROR_NONE);
	CHECK(status() == MB_OTA_ST_COMMITTED);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_ABORT) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(write_header(n, cs, MB_OTA_PSK) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);
	CHECK(status() == MB_OTA_ST_COMMITTED);
	CHECK(erased == 1);

	/* 10. COMMIT gui lai (PC mat phan hoi lan dau) -> NMBS_ERROR_NONE, khong goi ops->commit them lan nua */
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_ERROR_NONE);
	CHECK(committed == 1 && status() == MB_OTA_ST_COMMITTED);

	/* 11. Khoi voi offset >= bin_len -> loi offset chung: exception 4, STATUS 0x8002 */
	setup(); n = 1000; cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	CHECK(send_chunk(n, image, 128) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
	CHECK(status() == MB_OTA_ST_ERR_OFFSET);

	/* 12. mb_ota_init(NULL) (chua goi voi ops that) -> ghi header khong dung ops nen OK,
	 * nhung BEGIN phai tra loi thiet bi hong thay vi crash NULL */
	setup(); mb_ota_init(NULL); n = 1000; cs = img_checksum(image, n);
	CHECK(write_header(n, cs, MB_OTA_PSK) == NMBS_ERROR_NONE);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

	/* 13. Broadcast (unit_id 0) khong duoc kich hoat OTA: client dich chi 0 gui BEGIN sau
	 * header hop le -> nanoMODBUS tra NMBS_ERROR_NONE ngay (broadcast khong cho phan hoi),
	 * nhung server KHONG duoc xoa flash */
	setup(); n = 1000; cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	{
		nmbs_t bclient;
		fake_make_client(&bclient, 0);	/* dia chi dich 0 = broadcast RTU */
		CHECK(nmbs_write_single_register(&bclient, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN) == NMBS_ERROR_NONE);
		nmbs_server_poll(&server);	/* xu ly yeu cau broadcast dang nam trong hang doi */
	}
	CHECK(erased == 0 && status() == MB_OTA_ST_IDLE);

	printf("test_ota: %s\n", fake_failures ? "FAIL" : "OK");
	return fake_failures ? 1 : 0;
}
