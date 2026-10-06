/**
 * Firmware update + bootloader tests on host: crc, header, STAGING writer,
 * protocol, boot_decide, boot_install, power cut at EVERY flash operation.
 */
#include <string.h>
#include <setjmp.h>

#include "hal.h"
#include "crc.h"
#include "boot_ctrl.h"
#include "boot_core.h"
#include "fw_image.h"
#include "fw_update.h"
#include "fw_proto.h"
#include "port_host.h"
#include "tiny_test.h"

extern void boot_main(void);

static uint8_t img_buf[0x10000];

/*----------------------------------------------------------------------------
 * helper
 *--------------------------------------------------------------------------*/
static uint32_t make_image(uint8_t* out, uint32_t img_size, uint8_t maj, uint8_t min, uint8_t pat,
						   const char* board, uint32_t seed) {
	fw_image_hdr_t* h = (fw_image_hdr_t*)out;
	uint32_t app_addr = hal_flash_info(FLASH_PART_APP)->addr;
	uint8_t* p = out + FW_IMAGE_HDR_SIZE;
	uint32_t x = seed * 2654435761UL + 1;
	uint32_t sp = HOST_RAM_END;
	uint32_t rst = app_addr + FW_IMAGE_HDR_SIZE + 0x101;

	memset(out, 0, FW_IMAGE_HDR_SIZE);
	for (uint32_t i = 0; i < img_size; i++) {
		x = x * 1103515245UL + 12345UL;
		p[i] = (uint8_t)(x >> 16);
	}
	memcpy(p, &sp, 4);
	memcpy(p + 4, &rst, 4);

	h->magic = FW_IMAGE_MAGIC;
	h->hdr_version = FW_IMAGE_HDR_VERSION;
	h->hdr_size = FW_IMAGE_HDR_SIZE;
	h->img_size = img_size;
	h->img_crc32 = crc32_update(CRC32_INIT, p, img_size);
	h->load_addr = app_addr + FW_IMAGE_HDR_SIZE;
	h->version.major = maj;
	h->version.minor = min;
	h->version.patch = pat;
	h->version.build = seed;
	strncpy(h->board, board, FW_IMAGE_BOARD_LEN);
	h->hdr_crc32 = crc32_update(CRC32_INIT, h, FW_IMAGE_HDR_SIZE - 4);
	return FW_IMAGE_HDR_SIZE + img_size;
}

static void rehdr(uint8_t* out) {
	fw_image_hdr_t* h = (fw_image_hdr_t*)out;
	h->hdr_crc32 = crc32_update(CRC32_INIT, h, FW_IMAGE_HDR_SIZE - 4);
}

static void put_part(flash_part_t part, const uint8_t* data, uint32_t len) {
	const flash_part_info_t* p = hal_flash_info(part);
	memcpy(host_flash_mem() + (p->addr - HOST_FLASH_BASE), data, len);
}

static fw_err_t upload(const uint8_t* img, uint32_t total, uint32_t chunk) {
	fw_err_t e = fw_update_begin(total);
	for (uint32_t off = 0; e == FW_OK && off < total; off += chunk) {
		uint32_t n = (total - off < chunk) ? total - off : chunk;
		e = fw_update_write(off, img + off, n);
	}
	return e != FW_OK ? e : fw_update_finish(0);
}

static void setup(uint8_t erased) {
	host_reset_state(erased);
	host_use_virtual_time(1);
	host_fatal_jmp = 0;
	host_reset_handler = 0;
	host_jump_handler = 0;
	hal_init();
	fw_update_abort();
}

static uint8_t part_version(flash_part_t part, fw_err_t* err) {
	fw_image_hdr_t h;
	*err = fw_image_verify(part, &h);
	return (*err == FW_OK) ? h.version.major : 0;
}

/*----------------------------------------------------------------------------
 * run boot_main() like a real MCU: jump / reset / power cut -> longjmp here
 *--------------------------------------------------------------------------*/
enum { BOOT_JUMPED = 1, BOOT_RESET = 2, BOOT_CUT = 3, BOOT_FATAL = 4 };
static jmp_buf boot_jmp;
static uint32_t jumped_addr;

static void on_jump(uint32_t a) {
	jumped_addr = a;
	longjmp(boot_jmp, BOOT_JUMPED);
}

static void on_reset(void) {
	longjmp(boot_jmp, BOOT_RESET);
}

static jmp_buf cut_jmp;
static jmp_buf fatal_jmp;

static int run_boot(uint32_t power_cut_after) {
	volatile int r;

	host_jump_handler = on_jump;
	host_reset_handler = on_reset;
	host_fatal_jmp = &fatal_jmp;
	jumped_addr = 0;

	if ((r = setjmp(boot_jmp)) != 0) {
		goto out;
	}
	if (setjmp(cut_jmp) != 0) {
		r = BOOT_CUT;
		goto out;
	}
	if (setjmp(fatal_jmp) != 0) {
		r = BOOT_FATAL;
		goto out;
	}

	host_flash_power_cut(power_cut_after, &cut_jmp);
	fw_update_abort();
	boot_main();
	r = 0;

out:
	host_flash_power_cut(0, 0);
	host_fatal_jmp = 0;
	return r;
}

static void inject_frame(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t len) {
	uint8_t f[FW_PROTO_MAX_PAYLOAD + 8];
	host_console_inject(f, fw_proto_pack(f, cmd, seq, payload, len));
}

static uint32_t app_entry(void) {
	return hal_flash_info(FLASH_PART_APP)->addr + FW_IMAGE_HDR_SIZE;
}

/*----------------------------------------------------------------------------
 * test
 *--------------------------------------------------------------------------*/
static void test_crc(void) {
	CHECK_EQ(crc32_update(0, "123456789", 9), 0xCBF43926UL);
	CHECK_EQ(crc16_update(0xFFFF, "123456789", 9), 0x29B1);
	/* incremental == one shot */
	CHECK_EQ(crc32_update(crc32_update(0, "1234", 4), "56789", 5), 0xCBF43926UL);
}

static void test_upload_ok(void) {
	fw_image_hdr_t h;
	uint32_t total;

	for (int erased = 0; erased < 2; erased++) {
		setup(erased ? 0xFF : 0x00);	/* 0x00 = STM32L1 */
		total = make_image(img_buf, 5001, 2, 3, 4, HOST_BOARD_NAME, 7);
		CHECK_EQ(upload(img_buf, total, 128), FW_OK);
		CHECK_EQ(fw_update_state(), FW_UPDATE_READY);
		CHECK_EQ(fw_image_verify(FLASH_PART_STAGING, &h), FW_OK);
		CHECK_EQ(h.version.minor, 3);
		CHECK_EQ(h.img_size, 5001);
	}
}

static void test_upload_errors(void) {
	uint32_t total;
	fw_image_hdr_t* h = (fw_image_hdr_t*)img_buf;

	setup(0xFF);
	total = make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);

	CHECK_EQ(fw_update_begin(hal_flash_info(FLASH_PART_STAGING)->size + 1), FW_ERR_TOO_BIG);
	CHECK_EQ(fw_update_begin(100), FW_ERR_SIZE);
	CHECK_EQ(fw_update_write(0, img_buf, 4), FW_ERR_STATE);

	CHECK_EQ(fw_update_begin(total), FW_OK);
	CHECK_EQ(fw_update_write(4, img_buf, 4), FW_ERR_OFFSET);
	CHECK_EQ(fw_update_write(0, img_buf, 3), FW_ERR_SIZE);	/* unaligned chunk that is not the last */
	CHECK_EQ(fw_update_write(0, img_buf, 128), FW_OK);
	CHECK_EQ(fw_update_finish(0), FW_ERR_STATE);			/* chua du */

	/* corrupt one binary byte */
	img_buf[FW_IMAGE_HDR_SIZE + 1000] ^= 0x01;
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_IMG_CRC);
	img_buf[FW_IMAGE_HDR_SIZE + 1000] ^= 0x01;
	CHECK_EQ(upload(img_buf, total, 128), FW_OK);

	/* sai board */
	strcpy(h->board, "other-board");
	rehdr(img_buf);
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_BOARD);

	/* wrong load address */
	make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);
	h->load_addr += 0x100;
	rehdr(img_buf);
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_ADDR);

	/* header crc */
	make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);
	h->version.major = 9;
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_HDR_CRC);

	/* no magic */
	make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);
	h->magic = 0;
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_MAGIC);

	/* bad vector table (wrong link address): rejected despite valid CRC */
	make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);
	{
		uint32_t bad = 0x08000101UL;
		memcpy(img_buf + FW_IMAGE_HDR_SIZE + 4, &bad, 4);
		h->img_crc32 = crc32_update(0, img_buf + FW_IMAGE_HDR_SIZE, 3000);
		rehdr(img_buf);
	}
	CHECK_EQ(upload(img_buf, total, 128), FW_ERR_VECTOR);

	/* header img_size does not match the uploaded size */
	make_image(img_buf, 3000, 1, 0, 0, HOST_BOARD_NAME, 1);
	CHECK_EQ(upload(img_buf, total + 4, 128), FW_ERR_SIZE);
}

static void test_boot_decide(void) {
	boot_state_t s;
	const uint8_t M = BOOT_MAX_INSTALL_ATTEMPTS;

#define DECIDE(cmd_, att_, app_, raw_, stg_) \
	(s.cmd = (cmd_), s.install_attempts = (att_), s.app_ok = (app_), \
	 s.app_raw_ok = (raw_), s.staging_ok = (stg_), boot_decide(&s))

	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 0, 1, 0, 0), BOOT_ACT_RUN);
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 0, 1, 0, 1), BOOT_ACT_RUN);
	CHECK_EQ(DECIDE(BOOT_CMD_UPDATE, 0, 1, 0, 1), BOOT_ACT_INSTALL);
	CHECK_EQ(DECIDE(BOOT_CMD_UPDATE, 0, 1, 0, 0), BOOT_ACT_RUN);		/* bad staging: keep running old app */
	CHECK_EQ(DECIDE(BOOT_CMD_UPDATE, M, 1, 0, 1), BOOT_ACT_RUN);		/* attempts exhausted */
	CHECK_EQ(DECIDE(BOOT_CMD_UPDATE, M, 0, 1, 1), BOOT_ACT_LOADER);	/* NEVER run a half-copied image */
	CHECK_EQ(DECIDE(BOOT_CMD_UPDATE, 1, 0, 1, 1), BOOT_ACT_INSTALL);	/* resume install after power loss */
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 0, 0, 0, 1), BOOT_ACT_INSTALL);		/* recover from staging */
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 0, 0, 1, 1), BOOT_ACT_RUN_RAW);		/* debug SWD flash */
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 2, 0, 1, 1), BOOT_ACT_INSTALL);		/* install pending: no raw run */
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, M, 0, 1, 1), BOOT_ACT_LOADER);
	CHECK_EQ(DECIDE(BOOT_CMD_NONE, 0, 0, 0, 0), BOOT_ACT_LOADER);
	CHECK_EQ(DECIDE(BOOT_CMD_LOADER, 0, 1, 0, 1), BOOT_ACT_LOADER);
#undef DECIDE
}

static void test_boot_ctrl(void) {
	boot_ctrl_t c;

	setup(0xFF);
	boot_ctrl_load(&c);
	CHECK_EQ(c.cmd, BOOT_CMD_NONE);
	CHECK_EQ(boot_ctrl_set_cmd(BOOT_CMD_UPDATE), 0);
	boot_ctrl_load(&c);
	CHECK_EQ(c.cmd, BOOT_CMD_UPDATE);
	/* corrupt NVM -> defaults */
	host_nvm_mem()[5] ^= 0xFF;
	boot_ctrl_load(&c);
	CHECK_EQ(c.cmd, BOOT_CMD_NONE);
}

static void test_boot_runs_valid_app(void) {
	uint32_t n;

	setup(0xFF);
	n = make_image(img_buf, 4000, 1, 0, 0, HOST_BOARD_NAME, 1);
	put_part(FLASH_PART_APP, img_buf, n);
	CHECK_EQ(run_boot(0), BOOT_JUMPED);
	CHECK_EQ(jumped_addr, app_entry());
}

static void test_boot_update_flow(void) {
	uint32_t n;
	fw_err_t e;
	boot_ctrl_t c;

	setup(0xFF);
	n = make_image(img_buf, 4000, 1, 0, 0, HOST_BOARD_NAME, 1);
	put_part(FLASH_PART_APP, img_buf, n);
	n = make_image(img_buf, 9000, 2, 0, 0, HOST_BOARD_NAME, 2);
	CHECK_EQ(upload(img_buf, n, 128), FW_OK);
	CHECK_EQ(boot_ctrl_set_cmd(BOOT_CMD_UPDATE), 0);

	CHECK_EQ(run_boot(0), BOOT_JUMPED);
	CHECK_EQ(part_version(FLASH_PART_APP, &e), 2);
	boot_ctrl_load(&c);
	CHECK_EQ(c.cmd, BOOT_CMD_NONE);
	CHECK_EQ(c.install_count, 1);
	CHECK_EQ(c.install_attempts, 0);

	/* reboot: run directly, no reinstall */
	CHECK_EQ(run_boot(0), BOOT_JUMPED);
	boot_ctrl_load(&c);
	CHECK_EQ(c.install_count, 1);
}

/* Power cut at EVERY erase/write of an install. After each cut, reboot until
 * the app runs: it must always be v2, never a partial image. */
static void test_boot_power_cut_everywhere(void) {
	uint32_t n_old, n_new, total_ops, k;
	int failures = 0;
	static uint8_t old_img[0x10000];
	fw_err_t e;

	/* count flash ops of one complete install */
	setup(0xFF);
	n_old = make_image(old_img, 6000, 1, 0, 0, HOST_BOARD_NAME, 11);
	n_new = make_image(img_buf, 7000, 2, 0, 0, HOST_BOARD_NAME, 22);
	put_part(FLASH_PART_APP, old_img, n_old);
	put_part(FLASH_PART_STAGING, img_buf, n_new);
	boot_ctrl_set_cmd(BOOT_CMD_UPDATE);
	{
		uint32_t before = host_flash_ops();
		CHECK_EQ(run_boot(0), BOOT_JUMPED);
		total_ops = host_flash_ops() - before;
	}
	CHECK(total_ops > 100);

	for (k = 1; k <= total_ops; k++) {
		int r, boots = 0;

		setup(0xFF);
		put_part(FLASH_PART_APP, old_img, n_old);
		put_part(FLASH_PART_STAGING, img_buf, n_new);
		boot_ctrl_set_cmd(BOOT_CMD_UPDATE);

		r = run_boot(k);
		while (r == BOOT_CUT && boots < 5) {
			r = run_boot(0);	/* power back on */
			boots++;
		}
		if (r != BOOT_JUMPED || part_version(FLASH_PART_APP, &e) != 2) {
			failures++;
			if (failures < 5) {
				printf("    cut at op %u: r=%d app=%s\n", k, r, fw_err_str(e));
			}
		}
	}
	printf("    %u cut points tested\n", total_ops);
	CHECK_EQ(failures, 0);
}

/* Three installs in a row cut by power loss -> attempts exhausted: enter the
 * loader, NEVER run the partial image (no header, but a sane vector table). */
static void test_boot_attempts_exhausted(void) {
	uint32_t n;
	boot_ctrl_t c;

	setup(0xFF);
	n = make_image(img_buf, 7000, 2, 0, 0, HOST_BOARD_NAME, 22);
	put_part(FLASH_PART_STAGING, img_buf, n);
	boot_ctrl_set_cmd(BOOT_CMD_UPDATE);

	for (int i = 0; i < BOOT_MAX_INSTALL_ATTEMPTS; i++) {
		CHECK_EQ(run_boot(20), BOOT_CUT);
	}
	boot_ctrl_load(&c);
	CHECK_EQ(c.install_attempts, BOOT_MAX_INSTALL_ATTEMPTS);

	/* loader: send RESET to leave the loop */
	inject_frame(FW_PROTO_CMD_RESET, 1, 0, 0);
	CHECK_EQ(run_boot(0), BOOT_RESET);
}

/* Blank board: boot enters the loader, image is uploaded via the protocol
 * (like tools/ak_fw.py), INSTALL -> boot installs and runs it. */
static void test_loader_full_flow(void) {
	uint32_t n, off;
	uint8_t pl[4 + FW_PROTO_MAX_CHUNK];
	uint8_t seq = 0;
	uint8_t rx[4096];
	uint32_t rxn;
	fw_err_t e;

	setup(0xFF);
	n = make_image(img_buf, 3333, 3, 1, 0, HOST_BOARD_NAME, 5);

	inject_frame(FW_PROTO_CMD_INFO, seq++, 0, 0);
	pl[0] = (uint8_t)n; pl[1] = (uint8_t)(n >> 8); pl[2] = (uint8_t)(n >> 16); pl[3] = (uint8_t)(n >> 24);
	inject_frame(FW_PROTO_CMD_BEGIN, seq++, pl, 4);
	for (off = 0; off < n; off += FW_PROTO_MAX_CHUNK) {
		uint32_t len = (n - off < FW_PROTO_MAX_CHUNK) ? n - off : FW_PROTO_MAX_CHUNK;
		pl[0] = (uint8_t)off; pl[1] = (uint8_t)(off >> 8); pl[2] = (uint8_t)(off >> 16); pl[3] = (uint8_t)(off >> 24);
		memcpy(pl + 4, img_buf + off, len);
		inject_frame(FW_PROTO_CMD_DATA, seq++, pl, (uint16_t)(4 + len));
	}
	inject_frame(FW_PROTO_CMD_END, seq++, 0, 0);
	inject_frame(FW_PROTO_CMD_INSTALL, seq++, 0, 0);

	CHECK_EQ(run_boot(0), BOOT_JUMPED);
	CHECK_EQ(part_version(FLASH_PART_APP, &e), 3);

	/* INFO response: role BOOT; every response has status OK */
	rxn = host_console_take_tx(rx, sizeof(rx));
	{
		uint32_t i = 0, frames = 0, bad = 0;
		int saw_info = 0;
		while (i + 7 <= rxn) {
			if (rx[i] != FW_PROTO_SOF) {
				i++;
				continue;
			}
			uint16_t len = (uint16_t)(rx[i + 3] | (rx[i + 4] << 8));
			uint16_t crc = crc16_update(0xFFFF, rx + i + 1, 4U + len);
			if (i + 7 + len > rxn || crc != (uint16_t)(rx[i + 5 + len] | (rx[i + 6 + len] << 8))) {
				i++;
				continue;
			}
			frames++;
			if (rx[i + 5] != FW_OK) {
				bad++;
			}
			if (rx[i + 1] == (FW_PROTO_CMD_INFO | FW_PROTO_RESP)) {
				saw_info = 1;
				CHECK_EQ(rx[i + 7], FW_ROLE_BOOT);
			}
			i += 7U + len;
		}
		CHECK_EQ(frames, (uint32_t)seq);
		CHECK_EQ(bad, 0);
		CHECK(saw_info);
	}
}

/* Bad-CRC frames are silently dropped; interrupted frames are discarded after
 * the timeout; non-protocol text bytes return 0 (go to the shell). */
static void resp_tx(const uint8_t* d, uint32_t n) {
	while (n--) {
		hal_console_putc(*d++);
	}
}

static void test_proto_robustness(void) {
	static const fw_version_t v = { 1, 2, 3, 0, 4 };
	static const fw_proto_cfg_t cfg = { FW_ROLE_APP, &v, resp_tx };
	uint8_t f[16];
	uint32_t n;

	setup(0xFF);
	fw_proto_init(&cfg);
	CHECK_EQ(fw_proto_feed('h', 0), 0);
	CHECK_EQ(fw_proto_busy(), 0);

	n = fw_proto_pack(f, FW_PROTO_CMD_INFO, 9, 0, 0);
	f[n - 1] ^= 0xFF;	/* bad crc */
	for (uint32_t i = 0; i < n; i++) {
		CHECK_EQ(fw_proto_feed(f[i], 0), 1);
	}
	CHECK_EQ(host_console_take_tx(0, 1000), 0);

	/* interrupted frame: 3 bytes, silence > timeout, next frame still accepted */
	n = fw_proto_pack(f, FW_PROTO_CMD_INFO, 10, 0, 0);
	fw_proto_feed(f[0], 0);
	fw_proto_feed(f[1], 1);
	fw_proto_feed(f[2], 2);
	for (uint32_t i = 0; i < n; i++) {
		fw_proto_feed(f[i], 1000 + i);
	}
	CHECK(host_console_take_tx(0, 1000) > 0);

	/* unknown command */
	n = fw_proto_pack(f, 0x55, 11, 0, 0);
	for (uint32_t i = 0; i < n; i++) {
		fw_proto_feed(f[i], 2000);
	}
	{
		uint8_t r[16];
		CHECK_EQ(host_console_take_tx(r, sizeof(r)), 8);
		CHECK_EQ(r[1], 0x55 | FW_PROTO_RESP);
		CHECK_EQ(r[5], FW_ERR_CMD);
	}

	/* INSTALL without a verified image -> STATE, no action */
	n = fw_proto_pack(f, FW_PROTO_CMD_INSTALL, 12, 0, 0);
	for (uint32_t i = 0; i < n; i++) {
		fw_proto_feed(f[i], 3000);
	}
	CHECK_EQ(fw_proto_take_action(), FW_PROTO_ACT_NONE);
}

/* APP has a header but bad CRC and STAGING holds an image: boot must recover,
 * not treat it as "raw" and run it. */
static void test_boot_recover_corrupt_app(void) {
	uint32_t n;
	fw_err_t e;

	setup(0xFF);
	n = make_image(img_buf, 5000, 4, 0, 0, HOST_BOARD_NAME, 9);
	put_part(FLASH_PART_APP, img_buf, n);
	put_part(FLASH_PART_STAGING, img_buf, n);
	host_flash_mem()[(hal_flash_info(FLASH_PART_APP)->addr - HOST_FLASH_BASE) + 2000] ^= 0x10;
	CHECK_EQ(part_version(FLASH_PART_APP, &e), 0);
	CHECK_EQ(e, FW_ERR_IMG_CRC);

	CHECK_EQ(run_boot(0), BOOT_JUMPED);
	CHECK_EQ(part_version(FLASH_PART_APP, &e), 4);
}

/* Recovery path (cmd NONE, APP corrupt, STAGING good) cut at every step:
 * must never jump into a partial image. */
static void test_boot_recover_power_cut_everywhere(void) {
	static uint8_t bad_app[0x10000];
	uint32_t n, total_ops, k;
	int failures = 0;
	fw_err_t e;

	setup(0xFF);
	n = make_image(img_buf, 5000, 4, 0, 0, HOST_BOARD_NAME, 9);
	memcpy(bad_app, img_buf, n);
	bad_app[3000] ^= 0x10;
	put_part(FLASH_PART_APP, bad_app, n);
	put_part(FLASH_PART_STAGING, img_buf, n);
	{
		uint32_t before = host_flash_ops();
		CHECK_EQ(run_boot(0), BOOT_JUMPED);
		total_ops = host_flash_ops() - before;
	}

	for (k = 1; k <= total_ops; k++) {
		int r, boots = 0;

		setup(0xFF);
		put_part(FLASH_PART_APP, bad_app, n);
		put_part(FLASH_PART_STAGING, img_buf, n);
		r = run_boot(k);
		while (r == BOOT_CUT && boots < 5) {
			r = run_boot(0);
			boots++;
		}
		if (r != BOOT_JUMPED || part_version(FLASH_PART_APP, &e) != 4) {
			failures++;
		}
	}
	printf("    %u cut points tested\n", total_ops);
	CHECK_EQ(failures, 0);
}

TT_MAIN_BEGIN("test_fw")
	RUN_TEST(test_crc);
	RUN_TEST(test_upload_ok);
	RUN_TEST(test_upload_errors);
	RUN_TEST(test_boot_decide);
	RUN_TEST(test_boot_ctrl);
	RUN_TEST(test_proto_robustness);
	RUN_TEST(test_boot_runs_valid_app);
	RUN_TEST(test_boot_update_flow);
	RUN_TEST(test_boot_recover_corrupt_app);
	RUN_TEST(test_loader_full_flow);
	RUN_TEST(test_boot_attempts_exhausted);
	RUN_TEST(test_boot_power_cut_everywhere);
	RUN_TEST(test_boot_recover_power_cut_everywhere);
TT_MAIN_END()
