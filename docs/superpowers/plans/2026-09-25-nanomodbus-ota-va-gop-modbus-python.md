# nanoMODBUS + OTA Modbus (firmware AK) và gộp Modbus master Python — Kế hoạch triển khai

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Thay thư viện Modbus thương mại `mbmaster-v2.9.6` trong ak-base-kit-pio bằng nanoMODBUS (MIT) có cả master lẫn slave, thêm OTA qua Modbus RS485 dùng lại bootloader sẵn có, và gộp các bản Modbus master Python về `epcb_applib.modbus`.

**Architecture:** Firmware: nanoMODBUS nguyên bản + lớp port USART2 (ring buffer, timeout theo `sys_ctrl_millis`) + các module C thuần (`mb_slave_regs.c`, `mb_ota.c`) test được trên PC bằng gcc qua "đường truyền giả". OTA ghi ảnh vào external flash rồi đi đúng đường BSF → bootloader hiện có. Python: `core/modbus_rtu.py` thành lớp tương thích gọi `epcb_applib.modbus`; thêm `epcb_applib.ota` gửi ảnh từ PC.

**Tech Stack:** C99/C++11 bare-metal STM32L151 (SPL), PlatformIO tại `D:\devcache\platformio`, gcc winlibs (test host), Python 3.14 + pytest, pyserial.

**Specs:**
- `ak-base-kit-pio/docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md`
- `D:/OneDrive/05_Shared_Libraries/epcb-applib/docs/specs/2026-09-25-gop-modbus-master-va-ota.md`

## Global Constraints

- PlatformIO: `$env:PLATFORMIO_CORE_DIR="D:\devcache\platformio"`, chạy `D:\devcache\platformio\penv\Scripts\pio.exe` từ thư mục `ak-base-kit-pio`.
- Mốc build trước khi sửa (25/09/2026): app Flash 58220 B / RAM 10016 B; boot Flash 6820 B / RAM 1240 B. Vùng app tối đa 118784 B, RAM 16384 B.
- **Bootloader (`sources/boot/`) không sửa.**
- nanoMODBUS v1.23.0, 3 file `nanomodbus.c`, `nanomodbus.h`, `LICENSE` chép nguyên, không sửa.
- Commit trong ak-base-kit-pio: `git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit ...` (repo chưa đặt danh tính). Mọi commit kết thúc bằng dòng `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Comment trong firmware viết tiếng Việt không dấu (đúng kiểu file sẵn có); code Python theo kiểu `epcb_applib` (docstring tiếng Việt không dấu, thông báo lỗi có dấu).
- Bảng thanh ghi OTA (base 0xF000) và luật offset/checksum lấy đúng từ spec firmware; checksum = tổng word 32-bit little-endian & 0xFFFF, ảnh đệm 0xFF cho đủ bội 4.
- `FIRMWARE_PSK = 0x1A2B3C4D`, vùng app tối đa `118784` byte.
- Không xóa file ngoài git: thư mục nào chưa có git thì `git init` + commit trước khi xóa.

---

## Sơ đồ file

**ak-base-kit-pio** (`S = sources/application`):
| File | Vai trò |
|---|---|
| `S/networks/nanomodbus/{nanomodbus.c,nanomodbus.h,LICENSE}` | thư viện gốc (mới) |
| `S/networks/mb_port/rs485_port.{c,h}` | port USART2: init, read, write, irq (mới, chỉ chạy trên chip) |
| `S/networks/mb_port/mb_slave_regs.{c,h}` | callback server: bảng thanh ghi demo + chuyển OTA (mới, test được trên PC) |
| `S/networks/mb_port/mb_ota.{c,h}` | máy trạng thái OTA qua thanh ghi 0xF000 (mới ở Task 7, test được trên PC) |
| `S/app/app_modbus.{cpp,h}` | tạo client/server nanoMODBUS, poll slave, nối ops OTA (mới) |
| `S/app/app_modbus_pull.{cpp,h}` | đổi sang nmbs_* (sửa) |
| `S/app/app.cpp`, `app.h`, `app_data.{cpp,h}`, `task_list.{cpp,h}`, `task_fw.{cpp,h}` | sửa |
| `S/platform/stm32l/system.c` | vector USART2/TIM4 (sửa) |
| `platformio.ini` | đường dẫn, cờ, env `app_mbslave` (sửa) |
| `tests_host/modbus/*` | test gcc trên PC (mới) |

**epcb-applib**: `src/epcb_applib/ota.py`, `tests/test_ota.py` (mới); `pyproject.toml`, `CHANGELOG`/README (sửa).
**EPCB_Modbus_RTU_Master**: `core/modbus_rtu.py` (viết lại thành lớp tương thích), `tests/_modbus_rtu_old.py`, `tests/test_modbus_rtu_compat.py` (mới), `.gitignore` (mới).

Thứ tự: Task 1–3 (1A) → Task 4–5 (Python gộp) → Task 6–9 (1B OTA).

---

### Task 1: Đưa nanoMODBUS vào + khung test host + mb_slave_regs

**Files:**
- Create: `sources/application/networks/nanomodbus/nanomodbus.c`, `nanomodbus.h`, `LICENSE` (chép từ scratchpad `nmb/` — tag v1.23.0 đã clone)
- Create: `sources/application/networks/mb_port/mb_slave_regs.h`, `mb_slave_regs.c`
- Create: `tests_host/modbus/fake_link.h`, `fake_link.c`, `test_slave_regs.c`, `run_tests.sh`

**Interfaces:**
- Produces: `extern uint16_t mb_slave_demo_regs[MB_SLAVE_DEMO_REG_COUNT];` (`MB_SLAVE_DEMO_REG_COUNT` = 3; [0] = major<<8|minor, [1] = patch, [2] = uptime giây), `void mb_slave_regs_bind(nmbs_callbacks* cb);`. Fake link: `fake_link_reset()`, `fake_link_attach_server(nmbs_t*)`, `fake_link_corrupt_next_response()`, `fake_client_read/write`, `fake_server_read/write` (chữ ký `nmbs_platform_conf.read/write`), `fake_make_client(nmbs_t*, uint8_t dest)`, `fake_make_server(nmbs_t*, uint8_t addr)`.

- [ ] **Step 1: Chép thư viện**

```bash
SP="/c/Users/VTNNTU~1/AppData/Local/Temp/claude/D--OneDrive-03-EPCB-Hardware-Design/8b37d5ad-cee5-46ba-8452-5439fa817f63/scratchpad/nmb"
D="sources/application/networks/nanomodbus"
mkdir -p "$D" && cp "$SP/nanomodbus.c" "$SP/nanomodbus.h" "$SP/LICENSE" "$D/"
```

- [ ] **Step 2: Viết `mb_slave_regs.h`**

```c
#ifndef __MB_SLAVE_REGS_H__
#define __MB_SLAVE_REGS_H__

/* Bang thanh ghi cua Modbus slave (TASK_MBSLAVE_EN). Thuan C, khong dung
 * phan cung -> test duoc tren PC (tests_host/modbus). */

#include <stdint.h>
#include "nanomodbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_SLAVE_DEMO_REG_COUNT		(3)

/* 0: phien ban app (major << 8 | minor), 1: patch, 2: uptime giay (u16).
 * App ghi vao, slave chi cho doc. */
extern uint16_t mb_slave_demo_regs[MB_SLAVE_DEMO_REG_COUNT];

extern void mb_slave_regs_bind(nmbs_callbacks* cb);

#ifdef __cplusplus
}
#endif

#endif /* __MB_SLAVE_REGS_H__ */
```

- [ ] **Step 3: Viết `mb_slave_regs.c`**

```c
#include "mb_slave_regs.h"

/* Chi bien dich khi bat slave: o che do master nanoMODBUS duoc build voi
 * NMBS_SERVER_DISABLED, struct nmbs_callbacks khong co cac truong ben duoi. */
#if defined (TASK_MBSLAVE_EN)

uint16_t mb_slave_demo_regs[MB_SLAVE_DEMO_REG_COUNT];

static nmbs_error cb_read_holding(uint16_t address, uint16_t quantity, uint16_t* registers_out,
								  uint8_t unit_id, void* arg) {
	(void)unit_id; (void)arg;

	if ((uint32_t)address + quantity > MB_SLAVE_DEMO_REG_COUNT) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}
	for (uint16_t i = 0; i < quantity; i++) {
		registers_out[i] = mb_slave_demo_regs[address + i];
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error cb_write_single(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
	(void)address; (void)value; (void)unit_id; (void)arg;
	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;	/* thanh ghi demo chi doc */
}

static nmbs_error cb_write_multi(uint16_t address, uint16_t quantity, const uint16_t* registers,
								 uint8_t unit_id, void* arg) {
	(void)address; (void)quantity; (void)registers; (void)unit_id; (void)arg;
	return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

void mb_slave_regs_bind(nmbs_callbacks* cb) {
	cb->read_holding_registers = cb_read_holding;
	cb->write_single_register = cb_write_single;
	cb->write_multiple_registers = cb_write_multi;
}

#endif /* TASK_MBSLAVE_EN */
```

- [ ] **Step 4: Viết fake link `tests_host/modbus/fake_link.h`**

```c
#ifndef FAKE_LINK_H
#define FAKE_LINK_H
/* Duong truyen RS485 gia: client va server nanoMODBUS trong CUNG tien trinh.
 * Khi client cho phan hoi ma hang doi rong, fake_client_read goi
 * nmbs_server_poll() mot lan de server xu ly request dang cho. */
#include <stdint.h>
#include "nanomodbus.h"

void fake_link_reset(void);
void fake_link_attach_server(nmbs_t* server);
void fake_link_corrupt_next_response(void);   /* lat 1 bit byte cuoi phan hoi ke tiep -> sai CRC */

int32_t fake_client_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_client_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_server_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t fake_server_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

void fake_make_client(nmbs_t* client, uint8_t dest);
void fake_make_server(nmbs_t* server, uint8_t address, const nmbs_callbacks* cb);

/* Assert toi gian: in dong loi va dem, main tra ma thoat != 0 neu co loi. */
extern int fake_failures;
#define CHECK(cond) do { if (!(cond)) { fake_failures++; \
	printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#endif
```

- [ ] **Step 5: Viết `tests_host/modbus/fake_link.c`**

```c
#include <stdio.h>
#include <string.h>
#include "fake_link.h"

#define Q_SIZE 1024
typedef struct { uint8_t d[Q_SIZE]; uint16_t head, tail; } queue_t;

static queue_t c2s, s2c;			/* client->server, server->client */
static nmbs_t* attached_server;
static int corrupt_next;
int fake_failures;

static void q_push(queue_t* q, const uint8_t* b, uint16_t n) {
	for (uint16_t i = 0; i < n; i++) { q->d[q->head] = b[i]; q->head = (q->head + 1) % Q_SIZE; }
}
static uint16_t q_pop(queue_t* q, uint8_t* b, uint16_t n) {
	uint16_t k = 0;
	while (k < n && q->tail != q->head) { b[k++] = q->d[q->tail]; q->tail = (q->tail + 1) % Q_SIZE; }
	return k;
}

void fake_link_reset(void) { memset(&c2s, 0, sizeof c2s); memset(&s2c, 0, sizeof s2c); attached_server = NULL; corrupt_next = 0; }
void fake_link_attach_server(nmbs_t* server) { attached_server = server; }
void fake_link_corrupt_next_response(void) { corrupt_next = 1; }

int32_t fake_client_read(uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg;
	uint16_t n = q_pop(&s2c, buf, count);
	if (n < count && attached_server && c2s.tail != c2s.head) {
		nmbs_server_poll(attached_server);
		n += q_pop(&s2c, buf + n, count - n);
	}
	return n;
}
int32_t fake_client_write(const uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg; q_push(&c2s, buf, count); return count;
}
int32_t fake_server_read(uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg; return q_pop(&c2s, buf, count);
}
int32_t fake_server_write(const uint8_t* buf, uint16_t count, int32_t t, void* arg) {
	(void)t; (void)arg;
	uint8_t tmp[300];
	memcpy(tmp, buf, count);
	if (corrupt_next && count > 0) { tmp[count - 1] ^= 0x01; corrupt_next = 0; }
	q_push(&s2c, tmp, count);
	return count;
}

void fake_make_client(nmbs_t* client, uint8_t dest) {
	nmbs_platform_conf conf;
	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = fake_client_read;
	conf.write = fake_client_write;
	nmbs_client_create(client, &conf);
	nmbs_set_destination_rtu_address(client, dest);
	nmbs_set_read_timeout(client, 100);
	nmbs_set_byte_timeout(client, 20);
}
void fake_make_server(nmbs_t* server, uint8_t address, const nmbs_callbacks* cb) {
	nmbs_platform_conf conf;
	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = fake_server_read;
	conf.write = fake_server_write;
	nmbs_server_create(server, address, &conf, cb);
	nmbs_set_read_timeout(server, 0);
	nmbs_set_byte_timeout(server, 20);
	fake_link_attach_server(server);
}
```

- [ ] **Step 6: Viết test `tests_host/modbus/test_slave_regs.c`**

```c
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
```

- [ ] **Step 7: Viết `tests_host/modbus/run_tests.sh`**

```bash
#!/usr/bin/env bash
# Test host cho lop Modbus cua ak-base-kit-pio. Chay: bash tests_host/modbus/run_tests.sh
set -e
cd "$(dirname "$0")"
N=../../sources/application/networks
CFLAGS="-std=c99 -Wall -Wextra -g -DTASK_MBSLAVE_EN -I$N/nanomodbus -I$N/mb_port -I."
mkdir -p build
build() { gcc $CFLAGS -o "build/$1.exe" "$1.c" fake_link.c "$N/nanomodbus/nanomodbus.c" "${@:2}"; }
build test_slave_regs "$N/mb_port/mb_slave_regs.c"
for t in build/*.exe; do "$t"; done
```

Thêm `tests_host/modbus/build/` vào `.gitignore` của repo.

- [ ] **Step 8: Chạy — phải FAIL trước khi có mb_slave_regs?** Thứ tự thực tế: chạy `bash tests_host/modbus/run_tests.sh` với `mb_slave_regs_bind` tạm để rỗng (comment 3 dòng gán) → Expected: các CHECK exception/đọc FAIL (server trả ILLEGAL_FUNCTION). Bỏ comment → chạy lại.
Expected cuối: `test_slave_regs: OK`, exit 0.

- [ ] **Step 9: Commit**

```bash
git add sources/application/networks/nanomodbus sources/application/networks/mb_port tests_host .gitignore
git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit -m "feat(modbus): them nanoMODBUS v1.23.0 (MIT), bang thanh ghi slave va test host

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Port USART2 + chuyển master sang nanoMODBUS + gỡ mbmaster

**Files:**
- Create: `sources/application/networks/mb_port/rs485_port.h`, `rs485_port.c`
- Create: `sources/application/app/app_modbus.h`, `app_modbus.cpp`
- Modify: `app_modbus_pull.h/.cpp`, `app.cpp` (khối include Modbus ~dòng 65-72, `MBM_SERIAL_*` ~dòng 80-81, khối init ~dòng 180-205), `app_data.h/.cpp` (xMBMMaster), `app.h` (chú thích USART2 + `#error` ~dòng 20-43), `platform/stm32l/system.c` (~dòng 32-34, 77-80, 158-162, 175-181), `platformio.ini`
- Delete: `sources/application/networks/mbmaster-v2.9.6/` (git rm)

**Interfaces:**
- Consumes: nanoMODBUS API (Task 1).
- Produces: `void rs485_port_init(uint32_t baudrate); int32_t rs485_port_read(uint8_t*, uint16_t, int32_t, void*); int32_t rs485_port_write(const uint8_t*, uint16_t, int32_t, void*); void rs485_port_irq(void);` · `extern nmbs_t app_mb; nmbs_error app_modbus_init(void); void app_modbus_poll(void);` · `APP_MB_BAUDRATE 9600, APP_MB_SLAVE_ADDR 1, APP_MB_READ_TIMEOUT_MS 500, APP_MB_BYTE_TIMEOUT_MS 20`.

- [ ] **Step 1: `rs485_port.h`**

```c
#ifndef __RS485_PORT_H__
#define __RS485_PORT_H__

/* Lop port nanoMODBUS tren USART2 + chan DIR RS485 (PA2/PA3, xem io_cfg.h).
 * Nhan: ngat RXNE day byte vao ring buffer. Gui: polling TXE/TC, bat DIR
 * truoc khi gui, tra DIR ve nhan khi TC. Khong dung timer (TIM4 bo trong). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void rs485_port_init(uint32_t baudrate);
extern int32_t rs485_port_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
extern int32_t rs485_port_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
extern void rs485_port_irq(void);	/* goi tu rs485_irq() trong system.c */

#ifdef __cplusplus
}
#endif

#endif /* __RS485_PORT_H__ */
```

- [ ] **Step 2: `rs485_port.c`**

```c
#include "rs485_port.h"

#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)

#include "stm32l1xx.h"
#include "io_cfg.h"
#include "sys_ctrl.h"

#define RS485_RX_BUF_SIZE		(256)

/* Mot nguoi ghi (ngat) - mot nguoi doc (vong chinh): head chi ngat ghi,
 * tail chi vong chinh ghi -> khong can khoa. */
static volatile uint8_t  rx_buf[RS485_RX_BUF_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

void rs485_port_init(uint32_t baudrate) {
	USART_InitTypeDef USART_InitStructure;

	io_uart_rs485_cfg();			/* clock, GPIO AF, NVIC */
	io_rs485_dir_mode_output();
	io_rs485_dir_low();				/* mac dinh: nhan */

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART_RS485, &USART_InitStructure);

	rx_head = rx_tail = 0;
	USART_ITConfig(USART_RS485, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART_RS485, ENABLE);
}

void rs485_port_irq(void) {
	if (USART_GetITStatus(USART_RS485, USART_IT_RXNE) != RESET) {
		uint8_t c = (uint8_t)USART_ReceiveData(USART_RS485);	/* doc DR xoa RXNE */
		uint16_t next = (uint16_t)((rx_head + 1) % RS485_RX_BUF_SIZE);
		if (next != rx_tail) {	/* day thi bo byte - khung se hong CRC, master gui lai */
			rx_buf[rx_head] = c;
			rx_head = next;
		}
	}
	/* ORE cung sinh ngat RXNE: doc SR (GetFlagStatus) roi DR de xoa, khong
	 * thi ngat goi lai mai. */
	if (USART_GetFlagStatus(USART_RS485, USART_FLAG_ORE) != RESET) {
		(void)USART_ReceiveData(USART_RS485);
	}
}

int32_t rs485_port_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	(void)arg;
	uint16_t n = 0;
	uint32_t t0 = sys_ctrl_millis();

	while (n < count) {
		if (rx_tail != rx_head) {
			buf[n++] = rx_buf[rx_tail];
			rx_tail = (uint16_t)((rx_tail + 1) % RS485_RX_BUF_SIZE);
			t0 = sys_ctrl_millis();		/* timeout tinh cho TUNG byte */
			continue;
		}
		if (byte_timeout_ms == 0) {
			break;
		}
		if (byte_timeout_ms > 0 && (uint32_t)(sys_ctrl_millis() - t0) >= (uint32_t)byte_timeout_ms) {
			break;
		}
		sys_ctrl_independent_watchdog_reset();
		sys_ctrl_soft_watchdog_reset();
	}
	return n;
}

int32_t rs485_port_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	(void)arg; (void)byte_timeout_ms;

	io_rs485_dir_high();
	for (uint16_t i = 0; i < count; i++) {
		while (USART_GetFlagStatus(USART_RS485, USART_FLAG_TXE) == RESET);
		USART_SendData(USART_RS485, buf[i]);
	}
	/* Cho byte cuoi ra het day roi moi tra DIR, khong thi cat mat stop bit. */
	while (USART_GetFlagStatus(USART_RS485, USART_FLAG_TC) == RESET);
	io_rs485_dir_low();
	return count;
}

#endif /* TASK_MBMASTER_EN || TASK_MBSLAVE_EN */
```

Kiểm khi build: `io_cfg.h` phải khai `io_uart_rs485_cfg()` (đã có ở io_cfg.c:718 — nếu header thiếu thì thêm `extern void io_uart_rs485_cfg();` cạnh `io_rs485_dir_mode_output` ~dòng 273) và `sys_ctrl.h` khai hai hàm watchdog reset (task_fw.cpp đang dùng — lấy đúng header nó include).

- [ ] **Step 3: `app_modbus.h`**

```c
#ifndef __APP_MODBUS_H__
#define __APP_MODBUS_H__

#include <stdint.h>
#include "nanomodbus.h"

#define APP_MB_BAUDRATE				(9600)
#define APP_MB_SLAVE_ADDR			(1)
#define APP_MB_READ_TIMEOUT_MS		(500)	/* master: cho phan hoi */
#define APP_MB_BYTE_TIMEOUT_MS		(20)	/* khoang lang toi da giua 2 byte trong 1 khung */

extern nmbs_t app_mb;

extern nmbs_error app_modbus_init(void);
extern void app_modbus_poll(void);	/* slave: goi tu task_polling_mbslave(); master: rong */

#endif /* __APP_MODBUS_H__ */
```

- [ ] **Step 4: `app_modbus.cpp`**

```cpp
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
	mb_slave_demo_regs[0] = (uint16_t)((APP_VER_MAJOR << 8) | APP_VER_MINOR);
	mb_slave_demo_regs[1] = (uint16_t)APP_VER_PATCH;
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
```

`APP_VER_MAJOR/MINOR/PATCH`: đọc `app.h` quanh dòng 210-222. Nếu build không qua `pio_build_flags.py` chúng không tồn tại → bọc bằng `#if defined(APP_VER_MAJOR) ... #else 0 #endif` giống cách `APP_VER` đang làm.

- [ ] **Step 5: Sửa `app_modbus_pull.h`** — bỏ 5 include mbmaster (`port.h`, `mbport.h`, `mbm.h`, `mbtypes.h`, `mbportlayer.h`), thay bằng `#include "nanomodbus.h"`; trong 2 struct đổi `USHORT regAddress` → `uint16_t`, `ULONG regValue` → `uint32_t`, `ULONG tBaud` → `uint32_t`. Chạy `grep -rn "USHORT\|ULONG\|UCHAR\|UBYTE" sources/application --include=*.cpp --include=*.h | grep -v "networks/mbmaster"` và sửa mọi chỗ còn lại (shell.cpp) sang kiểu stdint.

- [ ] **Step 6: Sửa `app_modbus_pull.cpp`** — include `app_modbus.h` thay cho biến `xMBMMaster`; thay prototype + hàm đọc; xóa khối `#if 0 ... appMBMasterWrite ... #endif` (cả prototype lẫn thân):

```cpp
static nmbs_error appMBMasterRead(uint8_t slAddr, uint8_t funCode, uint16_t addReg, uint16_t *buf);
```
Trong `updateDataModbusDevice`: `USHORT` → `uint16_t`; `eMBErrorCode errCode = ...; if (errCode != MB_ENOERR)` → `nmbs_error errCode = ...; if (errCode != NMBS_ERROR_NONE)`.

```cpp
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
```

- [ ] **Step 7: Sửa `app.cpp`** — xóa khối `#include "mbport.h" ... "common/mbportlayer.h"` và 2 dòng `#define MBM_SERIAL_*`; thêm `#include "app_modbus.h"`. Thay toàn bộ khối init `#if defined (TASK_MBMASTER_EN) /* modbus rtu init*/ ... switch ... #endif` (đọc hết tới `#endif` tương ứng) bằng:

```cpp
#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)
	/* modbus rtu init */
	nmbs_error mb_err = app_modbus_init();
	APP_PRINT("Init modbus %s >> %d\n",
#if defined (TASK_MBMASTER_EN)
			  "master",
#else
			  "slave",
#endif
			  (int)mb_err);
#endif
```

Thêm hàm polling cạnh `task_polling_zigbee()`:

```cpp
void task_polling_mbslave() {
#if defined (TASK_MBSLAVE_EN)
	app_modbus_poll();
#endif
}
```

- [ ] **Step 8: Sửa `app_data.h/.cpp`** — xóa `#include "mbm.h"`, `extern xMBHandle xMBMMaster;` và `xMBHandle xMBMMaster;` cùng khối `#if defined (TASK_MBMASTER_EN)` bao chúng.

- [ ] **Step 9: Sửa `task_list.h/.cpp`** — thêm `AC_TASK_POLLING_MBSLAVE_ID` vào enum ID polling ngay trước `AK_TASK_POLLING_EOT_ID`, `extern void task_polling_mbslave();` cạnh `task_polling_console`, và dòng bảng:

```cpp
	#if defined (TASK_MBSLAVE_EN)
	{AC_TASK_POLLING_MBSLAVE_ID	,	AK_ENABLE	,	task_polling_mbslave		},
	#else
	{AC_TASK_POLLING_MBSLAVE_ID	,	AK_DISABLE	,	task_polling_mbslave		},
	#endif
```

- [ ] **Step 10: Sửa `app.h`** — chú thích USART2: dòng `TASK_MBMASTER_EN   mbmaster giu USART2 (vector vMBPUSART2ISR, TIM4)` → hai dòng `TASK_MBMASTER_EN   nanoMODBUS master giu USART2 (vector rs485_irq)` / `TASK_MBSLAVE_EN    nanoMODBUS slave giu USART2 (vector rs485_irq)`. Thay `#error` cũ bằng:

```c
#if (defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)) && defined (SERIAL2_EN)
#error "USART2 chi co mot chu: TASK_MBMASTER_EN/TASK_MBSLAVE_EN va SERIAL2_EN (hoac TASK_ZIGBEE_EN) khong bat cung luc duoc"
#endif

#if defined (TASK_MBMASTER_EN) && defined (TASK_MBSLAVE_EN)
#error "TASK_MBMASTER_EN va TASK_MBSLAVE_EN cung dung USART2 - chi bat mot"
#endif
```

- [ ] **Step 11: Sửa `system.c`** — include: `#include "mbport.h"` → `#include "rs485_port.h"` với điều kiện `#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)`. Khai báo: thay `void vMBPTimerISR( void ); void vMBPUSART2ISR( void );` bằng `void rs485_irq();` cùng điều kiện đó. Bảng vector: khe TIM4 chỉ còn `default_handler,	//	TIM4` (bỏ nhánh `vMBPTimerISR`); khe USART2:

```c
		#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)
		rs485_irq,								//	USART2
		#elif defined (SERIAL2_EN)
```
Thêm hàm cạnh `uart2_irq()`:

```c
#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)
void rs485_irq() {
	task_entry_interrupt();
	rs485_port_irq();
	task_exit_interrupt();
}
#endif
```

- [ ] **Step 12: Sửa `platformio.ini` [env:app]** — xóa 8 dòng `-Isources/application/networks/mbmaster-v2.9.6/...` và 8 dòng `+<application/networks/mbmaster-v2.9.6/...>`; thêm:

```ini
    -Isources/application/networks/nanomodbus
    -Isources/application/networks/mb_port
```
```ini
    +<application/networks/nanomodbus/*.c>
    +<application/networks/mb_port/*.c>
```
Thêm cờ cạnh `-DTASK_MBMASTER_EN` (giảm flash, cờ phải là cờ TOÀN CỤC vì làm đổi layout `nmbs_callbacks`):

```ini
    -DNMBS_SERVER_DISABLED
    -DNMBS_STRERROR_DISABLED
```
Sửa chú thích module: "Bat: link UART, modbus master (nanoMODBUS), OLED SSD1309 ... Tat: ... TASK_MBSLAVE_EN (xem env:app_mbslave)".

- [ ] **Step 13: Gỡ mbmaster + tham chiếu Makefile cũ**

```bash
git rm -r -q sources/application/networks/mbmaster-v2.9.6
grep -rn "mbmaster" --include=*.mk --include=*.ini --include=*.py --include=*.c --include=*.cpp --include=*.h . | grep -v "^./docs"
```
Expected: chỉ còn dòng trong `networks/Makefile.mk` (Makefile gốc, không dùng khi build pio) → xóa dòng `-include .../mbmaster-v2.9.6/Makefile.mk` đó để hai hệ build không lệch nhau.

- [ ] **Step 14: Build và so kích thước**

```powershell
$env:PLATFORMIO_CORE_DIR="D:\devcache\platformio"; & D:\devcache\platformio\penv\Scripts\pio.exe run -e app -e boot
```
Expected: cả hai `SUCCESS`, không warning mới trong file đã sửa; ghi lại dòng `RAM:`/`Flash:` của app (mốc cũ 58220/10016). Boot phải giữ nguyên 6820/1240.

- [ ] **Step 15: Test host vẫn xanh:** `bash tests_host/modbus/run_tests.sh` → `test_slave_regs: OK`.

- [ ] **Step 16: Commit**

```bash
git add -A sources platformio.ini
git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit -m "feat(modbus): master chuyen sang nanoMODBUS, go mbmaster-v2.9.6 (thu vien thuong mai)

Port USART2 moi (ring buffer + DIR RS485), khong con dung TIM4.
Flash app: 58220 -> <so moi> B.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Env `app_mbslave`, tài liệu, phiên bản 1.2.0

**Files:** Modify `platformio.ini`, `CHANGELOG.md`, `README.md`, `docs/huong-dan-su-dung-source-base.md`, `D:/OneDrive/03_EPCB Hardware_Design/10_Tool_IOT/CLAUDE.md` (dòng "EPCB Frimware Source Base": `ak-base-kit-pio\` nay đã đủ mã nguồn).

- [ ] **Step 1: Thêm env** vào cuối phần app của `platformio.ini`:

```ini
; ------------------------------------------------------------------------------
; env:app_mbslave - bien the Modbus SLAVE (nanoMODBUS server tren USART2),
; dung cho OTA qua RS485. Giong env:app, chi doi master -> slave.
; ------------------------------------------------------------------------------
[env:app_mbslave]
extends = env:app
build_unflags =
    ${env.build_unflags}
    -DTASK_MBMASTER_EN
    -DNMBS_SERVER_DISABLED
build_flags =
    ${env:app.build_flags}
    -DTASK_MBSLAVE_EN
    -DNMBS_CLIENT_DISABLED
```
Mở `pio_copy_release.py`: nếu tên file release lấy từ `APP_TITLE` thì bản slave sẽ ghi đè bản master → thêm hậu tố env (`env["PIOENV"]`) khi env ≠ `app`. Nếu nó đã theo tên env thì không sửa.

- [ ] **Step 2: Build cả ba env**

```powershell
$env:PLATFORMIO_CORE_DIR="D:\devcache\platformio"; & D:\devcache\platformio\penv\Scripts\pio.exe run -e app -e app_mbslave -e boot
```
Expected: 3 × SUCCESS. Nếu lệnh `shell modbus` (shell.cpp, bọc `TASK_MBMASTER_EN`) báo lỗi trong env slave thì kiểm lại điều kiện bọc — không được gọi `updateDataModbusDevice` khi slave.

- [ ] **Step 3: Nâng phiên bản** `-DAPP_VERSION=\"1.1.2\"` → `\"1.2.0\"` ở cả `env:app` và `env:boot` (base đánh số chung). CHANGELOG mục `1.2.0 — 2026-09-25`: thay mbmaster (thương mại) bằng nanoMODBUS MIT; thêm chế độ slave `TASK_MBSLAVE_EN` + env `app_mbslave`; bỏ TIM4 khỏi Modbus; test host `tests_host/modbus`; số flash trước/sau.

- [ ] **Step 4: README / hướng dẫn** — bảng module bật/tắt: thêm `TASK_MBSLAVE_EN`, bảng thanh ghi demo (0/1/2), cách chạy `bash tests_host/modbus/run_tests.sh`, lệnh `pio run -e app_mbslave`.

- [ ] **Step 5: Build lại (phiên bản mới) + commit**

```bash
git add -A platformio.ini pio_copy_release.py CHANGELOG.md README.md docs
git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit -m "feat: env app_mbslave (Modbus slave), v1.2.0

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(`10_Tool_IOT/CLAUDE.md` không nằm trong repo — sửa tại chỗ, không commit.)

- [ ] **Step 6 (anh chạy trên board):** nạp `env:app`, mở console, gõ `modbus r` → ES35-SW (địa chỉ 2) ra nhiệt độ/độ ẩm hợp lý.

---

### Task 4: EPCB_Modbus_RTU_Master — git + lớp tương thích

**Files (thư mục `D:/OneDrive/03_EPCB Hardware_Design/10_Tool_IOT/EPCB_Modbus_RTU_Master`):**
- Create: `.gitignore`, `tests/_modbus_rtu_old.py` (bản cũ nguyên văn), `tests/test_modbus_rtu_compat.py`
- Modify: `core/modbus_rtu.py`

**Interfaces:**
- Produces (giữ nguyên tên/chữ ký cũ): `crc16(bytes)->int`, `build_write_single(addr, reg, value)->bytes`, `build_read_holding(addr, reg, count=1)`, `build_read_input(addr, reg, count=1)`, `check_crc(bytes)->bool`, `parse_write_single(resp, req)->bool`, `parse_read_holding(resp, count)->list|None`, `parse_read_input(resp, count)->list|None`, `hexs(bytes)->str`, `s16(int)->int`.

- [ ] **Step 1: git init + commit hiện trạng**

`.gitignore`:
```
__pycache__/
build/
dist/
installer_output/
logs/
*.spec
```
(`*.spec` bỏ qua vì .bat sinh lại — quy ước 10_Tool_IOT.)

```bash
cd "/d/OneDrive/03_EPCB Hardware_Design/10_Tool_IOT/EPCB_Modbus_RTU_Master"
git init -q && git add -A && git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit -q -m "chore: dua EPCB_Modbus_RTU_Master vao git (trang thai 25/09/2026)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 2: Giữ bản cũ làm chuẩn so sánh:** `mkdir -p tests && cp core/modbus_rtu.py tests/_modbus_rtu_old.py`

- [ ] **Step 3: Viết test so sánh `tests/test_modbus_rtu_compat.py`**

```python
# -*- coding: utf-8 -*-
"""core/modbus_rtu.py (lop tuong thich) phai cho KET QUA Y HET ban cu.

Ban cu nam o tests/_modbus_rtu_old.py, giu nguyen van de lam chuan.
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import _modbus_rtu_old as old  # noqa: E402
from core import modbus_rtu as new  # noqa: E402

R = random.Random(20260925)


def _crc_le(body):
    c = old.crc16(body)
    return body + bytes([c & 0xFF, c >> 8])


def _mutations(frame):
    """Khung dung + cac bien the hong: cat ngan, lat bit, doi fc, doi dia chi."""
    out = [frame, b"", frame[:3], frame[:-1], frame + b"\x00\x11"]
    for i in range(len(frame)):
        b = bytearray(frame)
        b[i] ^= 1 << R.randrange(8)
        out.append(bytes(b))
    return out


def test_crc16_va_hexs():
    for _ in range(300):
        d = bytes(R.randrange(256) for _ in range(R.randrange(0, 40)))
        assert new.crc16(d) == old.crc16(d)
        assert new.hexs(d) == old.hexs(d)
        assert new.check_crc(d) == old.check_crc(d)


def test_build_giong_tung_byte():
    for _ in range(500):
        a, reg, val = R.randrange(256), R.randrange(65536), R.randrange(65536)
        cnt = R.randrange(1, 126)
        assert new.build_write_single(a, reg, val) == old.build_write_single(a, reg, val)
        assert new.build_read_holding(a, reg, cnt) == old.build_read_holding(a, reg, cnt)
        assert new.build_read_input(a, reg, cnt) == old.build_read_input(a, reg, cnt)


def test_parse_read_giong_ca_khi_khung_hong():
    for fc, f_old, f_new in ((3, old.parse_read_holding, new.parse_read_holding),
                             (4, old.parse_read_input, new.parse_read_input)):
        for _ in range(150):
            cnt = R.randrange(1, 10)
            body = bytes([R.randrange(1, 248), fc, cnt * 2]) + bytes(
                R.randrange(256) for _ in range(cnt * 2))
            for f in _mutations(_crc_le(body)) + [_crc_le(bytes([body[0], fc | 0x80, 2]))]:
                for c in (cnt, cnt + 1):
                    assert f_new(f, c) == f_old(f, c), (fc, f.hex(), c)


def test_parse_write_single_giong_ca_khi_khung_hong():
    for _ in range(200):
        req = old.build_write_single(R.randrange(1, 248), R.randrange(65536), R.randrange(65536))
        for f in _mutations(req) + [_crc_le(bytes([req[0], 0x86, 3]))]:
            assert new.parse_write_single(f, req) == old.parse_write_single(f, req), f.hex()


def test_s16():
    for v in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 12345):
        assert new.s16(v) == old.s16(v)
```

- [ ] **Step 4: Chạy test với bản cũ còn nguyên** — `python -m pytest tests/test_modbus_rtu_compat.py -q` → Expected: PASS (so bản cũ với chính nó — xác nhận test chạy được). Sau đó tạm sửa `core/modbus_rtu.py` hàm `s16` trả `v` → chạy lại → Expected FAIL ở `test_s16` (test bắt được khác biệt), rồi hoàn tác.

- [ ] **Step 5: Viết lại `core/modbus_rtu.py`**

```python
# -*- coding: utf-8 -*-
"""
Modbus RTU helpers - LOP TUONG THICH.

Ruot la epcb_applib.modbus (ban chuan dung chung cho moi tool EPCB, co test,
co FC01..06/16). File nay chi giu dung ten + cach tra ve cu (int CRC, None
khi khung loi) de devices/*.py va app chinh khong phai sua.
Khac ban cu duy nhat: count ngoai 1..125 nem ValueError (ban cu tao khung sai
chuan); app doc theo khoi <= LORA_MAX_REGS nen khong cham toi.
Test doi chieu tung byte voi ban cu: tests/test_modbus_rtu_compat.py.
"""
from epcb_applib import modbus as _mb
from epcb_applib.modbus import hexs  # noqa: F401 - cung dinh dang "AA BB"

s16 = _mb.to_i16


def crc16(data: bytes) -> int:
    """CRC16-Modbus dang so nguyen (ban chuan tra 2 byte little-endian)."""
    return int.from_bytes(_mb.crc16(data), "little")


def build_write_single(addr, reg, value):
    """Function code 0x06 - Write Single Register."""
    return _mb.build_write_single(addr & 0xFF, reg & 0xFFFF, value, _mb.FC_WRITE_REG)


def build_read_holding(addr, reg, count=1):
    """Function code 0x03 - Read Holding Registers."""
    return _mb.build_read(addr & 0xFF, reg & 0xFFFF, count, _mb.FC_HOLDING)


def build_read_input(addr, reg, count=1):
    """Function code 0x04 - Read Input Registers (ES1-O2 de gia tri do o day)."""
    return _mb.build_read(addr & 0xFF, reg & 0xFFFF, count, _mb.FC_INPUT)


def check_crc(resp: bytes) -> bool:
    return len(resp) >= 4 and _mb.crc16(resp[:-2]) == bytes(resp[-2:])


def parse_write_single(resp: bytes, req: bytes) -> bool:
    """Phan hoi hop le cua 0x06 la echo dung request."""
    try:
        _, val = _mb.parse_write(resp, req[0], (req[2] << 8) | req[3], _mb.FC_WRITE_REG)
    except _mb.ModbusError:
        return False
    return val == ((req[4] << 8) | req[5])


def _parse_read(resp, count, fc):
    # Ban cu khong kiem dia chi slave -> lay chinh byte dau lam slave.
    if not resp:
        return None
    try:
        return _mb.parse_read(resp, resp[0], count, fc)
    except _mb.ModbusError:
        return None


def parse_read_holding(resp: bytes, count):
    return _parse_read(resp, count, _mb.FC_HOLDING)


def parse_read_input(resp: bytes, count):
    return _parse_read(resp, count, _mb.FC_INPUT)
```

- [ ] **Step 6: Chạy test so sánh** — `python -m pytest tests/test_modbus_rtu_compat.py -q` → Expected: 5 passed. Có ca lệch nào → sửa lớp tương thích cho khớp bản cũ (không sửa test), trừ khi lệch nằm đúng ở "count ngoài 1..125".

- [ ] **Step 7: Test panel + build exe** — `python test_build_panels.py` (đọc đầu file để biết cách chạy; nếu là pytest thì `python -m pytest test_build_panels.py -q`) → pass. Chạy `build_modbus_rtu_master_exe.bat` (nó tự chạy test trước) → exe mới trong `dist\`; mở exe, chọn cổng COM bất kỳ, xem cửa sổ lên không lỗi import.

- [ ] **Step 8: Commit** (repo EPCB_Modbus_RTU_Master)

```bash
git add core/modbus_rtu.py tests
git -c user.name="Hoang Anh" -c user.email="hohoanga@gmail.com" commit -m "refactor: core/modbus_rtu thanh lop tuong thich goi epcb_applib.modbus

Test doi chieu tung byte voi ban cu (tests/test_modbus_rtu_compat.py).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Bỏ các bản epcb_applib chép riêng

**Files:** thư mục `epcb_applib` chép riêng trong `10_Tool_IOT/Mock device`, `10_Tool_IOT/EPCB Purchase Tool`, `05_EPCB_Project/07_INF Project/Adapter_Test_2I2/04_Software`; `requirements.txt` của từng tool.

- [ ] **Step 1: Tìm đúng vị trí bản chép**

```bash
cd "/d/OneDrive/03_EPCB Hardware_Design"
find "10_Tool_IOT/Mock device" "10_Tool_IOT/EPCB Purchase Tool" "05_EPCB_Project/07_INF Project/Adapter_Test_2I2/04_Software" -type d -name epcb_applib -not -path "*/build/*" -not -path "*/dist/*"
```

- [ ] **Step 2: Với TỪNG bản — so với thư viện chung**

```bash
diff -rq --exclude=__pycache__ "<ban_chep>" /d/OneDrive/05_Shared_Libraries/epcb-applib/src/epcb_applib
diff -r --exclude=__pycache__ "<ban_chep>" /d/OneDrive/05_Shared_Libraries/epcb-applib/src/epcb_applib | head -200
```
Phân loại từng khác biệt: (a) thư viện chung mới hơn (bản chép chỉ thiếu tính năng) → an toàn; (b) bản chép có sửa riêng → **DỪNG, báo anh** danh sách hunk trước khi làm tiếp tool đó.

- [ ] **Step 3: Tool nào chưa có git → git init + commit hiện trạng** (như Task 4 Step 1, `.gitignore` gồm `__pycache__/ build/ dist/ *.spec`). Tool đã có git thì bỏ qua.

- [ ] **Step 4: Gỡ bản chép (loại a):** `git rm -r -q <ban_chep>`; thêm vào `requirements.txt` của tool:
```
# Thu vien dung chung - cai: pip install -e "D:/OneDrive/05_Shared_Libraries/epcb-applib[all]"
epcb-applib>=1.3.0
```
Nếu tool đó build exe với `--collect-submodules`/`--add-data` trỏ vào bản chép, sửa .bat trỏ sang gói đã cài (PyInstaller tự thấy gói editable; thêm `--collect-submodules epcb_applib` nếu exe thiếu module).

- [ ] **Step 5: Kiểm import từ gói chung**

```bash
cd "<thu_muc_tool>" && python -c "import epcb_applib, sys; print(epcb_applib.__file__)"
```
Expected: đường dẫn trỏ về `05_Shared_Libraries\epcb-applib\src\...` (không phải thư mục tool). Chạy test của tool nếu có (Mock device: `python -m pytest -q`; khởi động app bằng `START.bat`/`python ...` và đóng lại không lỗi).

- [ ] **Step 6: Commit từng tool** — `git commit -m "chore: bo ban epcb_applib chep rieng, dung thu vien chung >=1.3.0"` (kèm dòng Co-Authored-By). Cập nhật `10_Tool_IOT/CLAUDE.md` bảng danh mục: bỏ chữ "vendored" ở Purchase Tool và Mock device, sửa câu "(Purchase Tool và Mock device vẫn đang dùng bản chép.)".

---

### Task 6: task_fw — tách hàm external flash + lệnh commit hẹn giờ

**Files:** Modify `sources/application/app/task_fw.cpp`, `task_fw.h`, `app.h` (enum signal của task FW).

**Interfaces:**
- Produces (C linkage để `app_modbus.cpp` gọi): `void fw_ext_erase(void); void fw_ext_write(uint32_t offset, const uint8_t* data, uint16_t len); uint16_t fw_ext_checksum(uint32_t len); void fw_commit_app_later(const firmware_header_t* header);` và signal `FW_MB_OTA_COMMIT`.

- [ ] **Step 1: Khai báo trong `task_fw.h`**

```c
#include "sys_boot.h"

extern void fw_ext_erase(void);
extern void fw_ext_write(uint32_t offset, const uint8_t* data, uint16_t len);
extern uint16_t fw_ext_checksum(uint32_t len);
/* Luu header, hen FW_MB_OTA_COMMIT sau 200 ms (de phan hoi Modbus kip ra day)
 * roi moi ghi BSF + reset. */
extern void fw_commit_app_later(const firmware_header_t* header);
```

- [ ] **Step 2: Thêm signal** `FW_MB_OTA_COMMIT` vào cuối enum signal của task FW trong `app.h` (enum chứa `FW_UPDATE_REQ`, `FW_TRANSFER_REQ`, `FW_PACKED_TIMEOUT`...). Thêm `#define FW_MB_OTA_COMMIT_DELAY_MS (200)`.

- [ ] **Step 3: Viết các hàm trong `task_fw.cpp`** (đặt sau các biến static đầu file):

```cpp
void fw_ext_erase(void) {
	for (int i = 0; i < APP_FLASH_FIRMWARE_BLOCK_64K_SIZE; i++) {
		sys_ctrl_independent_watchdog_reset();
		sys_ctrl_soft_watchdog_reset();
		flash_erase_block_64k(APP_FLASH_FIRMWARE_START_ADDR + (FLASH_BLOCK_64K_SIZE * i));
	}
}

void fw_ext_write(uint32_t offset, const uint8_t* data, uint16_t len) {
	flash_write(APP_FLASH_FIRMWARE_START_ADDR + offset, (uint8_t*)data, len);
}

uint16_t fw_ext_checksum(uint32_t len) {
	uint32_t checksum_buffer = 0;
	uint32_t word = 0;

	for (uint32_t index = 0; index < len; index += sizeof(uint32_t)) {
		sys_ctrl_independent_watchdog_reset();
		sys_ctrl_soft_watchdog_reset();

		word = 0;
		flash_read(APP_FLASH_FIRMWARE_START_ADDR + index, (uint8_t*)&word, sizeof(uint32_t));
		checksum_buffer += word;
	}
	return (uint16_t)(checksum_buffer & 0xFFFF);
}

void fw_commit_app_later(const firmware_header_t* header) {
	memcpy(&firmware_header_file, header, sizeof(firmware_header_t));
	timer_set(AC_TASK_FW_ID, FW_MB_OTA_COMMIT, FW_MB_OTA_COMMIT_DELAY_MS, TIMER_ONE_SHOT);
}
```
(`firmware_header_file` là biến static sẵn có của task_fw.cpp — hàm phải đặt SAU khai báo của nó. Kiểm tên `flash_write` nhận `uint8_t*` không const → giữ cast.)

- [ ] **Step 4: Cho đường UART cũ dùng lại** — trong `FW_UPDATE_SM_OK` thay vòng `for ... flash_erase_block_64k` bằng `fw_ext_erase();`; trong `FW_TRANSFER_REQ` thay `flash_write(APP_FLASH_FIRMWARE_START_ADDR + bin_file_cursor, ...)` bằng `fw_ext_write(bin_file_cursor, firmware_packet, firmware_packet_len);` và khối tính checksum (từ `uint32_t checksum_buffer = 0;` tới `uint16_t checksum_calculated = ...;`) bằng `uint16_t checksum_calculated = fw_ext_checksum(firmware_header_file.bin_len);` (giữ các dòng APP_DBG in kết quả).

- [ ] **Step 5: Case mới** trong switch của task_fw, cạnh `FW_INTERNAL_UPDATE_APP_RES_OK`:

```cpp
	case FW_MB_OTA_COMMIT: {
		APP_DBG_SIG("FW_MB_OTA_COMMIT\n");
		/* anh da nhan du qua Modbus va dung checksum (mb_ota.c) */
		fw_update_app_req_c_external_flash_io_none(&firmware_header_file);
		sys_ctrl_delay_ms(100);
		sys_ctrl_reset();
	}
		break;
```

- [ ] **Step 6: Build** `pio run -e app -e app_mbslave` → SUCCESS; Flash app không tăng quá ~100 B so với Task 3 (chỉ đổi chỗ code).

- [ ] **Step 7: Commit** — `refactor(fw): tach fw_ext_erase/write/checksum, them FW_MB_OTA_COMMIT`.

---

### Task 7: mb_ota.c — máy trạng thái OTA + test host

**Files:**
- Create: `sources/application/networks/mb_port/mb_ota.h`, `mb_ota.c`, `tests_host/modbus/test_ota.c`
- Modify: `sources/application/networks/mb_port/mb_slave_regs.c` (chuyển vùng 0xF000), `tests_host/modbus/run_tests.sh`

**Interfaces:**
- Produces: `mb_ota_ops_t {erase, write, checksum, commit}`; `void mb_ota_init(const mb_ota_ops_t*); bool mb_ota_owns(uint16_t addr, uint16_t qty); nmbs_error mb_ota_read(uint16_t addr, uint16_t qty, uint16_t* out); nmbs_error mb_ota_write(uint16_t addr, uint16_t qty, const uint16_t* regs);` và các hằng `MB_OTA_REG_*`, `MB_OTA_ST_*`, `MB_OTA_CMD_*`, `MB_OTA_PSK`, `MB_OTA_MAX_LEN`.

- [ ] **Step 1: `mb_ota.h`**

```c
#ifndef __MB_OTA_H__
#define __MB_OTA_H__

/* OTA qua Modbus: may tram thai tren khoi holding register 0xF000.
 * Thuan C - thao tac flash/commit di qua mb_ota_ops_t de test tren PC.
 * Bang thanh ghi: docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md */

#include <stdbool.h>
#include <stdint.h>
#include "nanomodbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_OTA_REG_CMD			(0xF000)
#define MB_OTA_REG_STATUS		(0xF001)
#define MB_OTA_REG_LEN_HI		(0xF002)	/* F002..F006: len hi, len lo, checksum, psk hi, psk lo */
#define MB_OTA_REG_CHECKSUM		(0xF004)
#define MB_OTA_REG_PSK_HI		(0xF005)
#define MB_OTA_REG_PSK_LO		(0xF006)
#define MB_OTA_REG_RECV_HI		(0xF007)	/* F007..F008: so byte da nhan */
#define MB_OTA_REG_RECV_LO		(0xF008)
#define MB_OTA_REG_CHUNK		(0xF010)	/* F010..F011 offset, F012.. du lieu */
#define MB_OTA_REG_LAST			(0xF051)
#define MB_OTA_CHUNK_MAX_REGS	(64)		/* 128 byte du lieu */

#define MB_OTA_CMD_BEGIN		(1)
#define MB_OTA_CMD_COMMIT		(2)
#define MB_OTA_CMD_ABORT		(3)

#define MB_OTA_ST_IDLE			(0x0000)
#define MB_OTA_ST_RECEIVING		(0x0001)
#define MB_OTA_ST_COMMITTED		(0x0002)
#define MB_OTA_ST_ERR_HEADER	(0x8001)
#define MB_OTA_ST_ERR_OFFSET	(0x8002)
#define MB_OTA_ST_ERR_CHECKSUM	(0x8003)
#define MB_OTA_ST_ERR_SIZE		(0x8004)

#define MB_OTA_PSK				(0x1A2B3C4DUL)	/* = FIRMWARE_PSK (sys_boot.h) */
#define MB_OTA_MAX_LEN			(118784UL)		/* vung app 116K */

typedef struct {
	void (*erase)(void);
	void (*write)(uint32_t offset, const uint8_t* data, uint16_t len);
	uint16_t (*checksum)(uint32_t len);
	void (*commit)(uint32_t psk, uint32_t bin_len, uint16_t checksum);	/* hen reset, KHONG reset ngay */
} mb_ota_ops_t;

extern void mb_ota_init(const mb_ota_ops_t* ops);
extern bool mb_ota_owns(uint16_t address, uint16_t quantity);
extern nmbs_error mb_ota_read(uint16_t address, uint16_t quantity, uint16_t* out);
extern nmbs_error mb_ota_write(uint16_t address, uint16_t quantity, const uint16_t* regs);

#ifdef __cplusplus
}
#endif

#endif /* __MB_OTA_H__ */
```

- [ ] **Step 2: Test trước — `tests_host/modbus/test_ota.c`**

```c
#include <stdio.h>
#include <string.h>
#include "fake_link.h"
#include "mb_slave_regs.h"
#include "mb_ota.h"

/* external flash gia */
static uint8_t flash[MB_OTA_MAX_LEN + 8];
static int erased, committed;
static uint32_t commit_len;

static void f_erase(void) { memset(flash, 0xFF, sizeof flash); erased++; }
static void f_write(uint32_t off, const uint8_t* d, uint16_t n) { memcpy(flash + off, d, n); }
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
static uint8_t image[1000];

static uint16_t img_checksum(const uint8_t* d, uint32_t n) {	/* anh da la boi 4 */
	uint32_t s = 0;
	for (uint32_t i = 0; i < n; i += 4) s += (uint32_t)d[i] | d[i+1] << 8 | d[i+2] << 16 | (uint32_t)d[i+3] << 24;
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
	erased = committed = 0;
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
		r[2 + i] = (uint16_t)(hi << 8 | lo);
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
	uint16_t cs = img_checksum(image, n);

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

	/* 2. Gui lai khoi vua gui (mat phan hoi) -> OK, khong ghi lai; nhay coc -> exception 4 */
	setup(); cs = img_checksum(image, n);
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	CHECK(send_chunk(0, image, 128) == NMBS_ERROR_NONE);
	CHECK(send_chunk(0, image, 128) == NMBS_ERROR_NONE);			/* gui lai */
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
	setup();
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

	/* 6. Anh le byte: khoi cuoi 3 byte, slave chi ghi toi bin_len */
	setup(); n = 1003; memset(flash, 0xFF, sizeof flash);
	{ uint8_t pad[1004]; memcpy(pad, image, 1003); pad[1003] = 0xFF; cs = img_checksum(pad, 1004); }
	write_header(n, cs, MB_OTA_PSK);
	nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_BEGIN);
	send_all(n);
	CHECK(flash[1003] == 0xFF);
	CHECK(nmbs_write_single_register(&client, MB_OTA_REG_CMD, MB_OTA_CMD_COMMIT) == NMBS_ERROR_NONE);

	/* 7. Vector checksum dung chung voi Python: byte 0..9 dem 0xFF -> 0x0F0C */
	{ uint8_t v[12] = {0,1,2,3,4,5,6,7,8,9,0xFF,0xFF}; CHECK(img_checksum(v, 12) == 0x0F0C); }

	/* 8. Thanh ghi demo van doc duoc ben canh vung OTA */
	{ uint16_t r; CHECK(nmbs_read_holding_registers(&client, 0, 1, &r) == NMBS_ERROR_NONE); }

	printf("test_ota: %s\n", fake_failures ? "FAIL" : "OK");
	return fake_failures ? 1 : 0;
}
```

Ghi chú ca 6: checksum slave tính trên `bin_len` làm tròn lên bội 4 (flash sau bin_len là 0xFF do đã xóa) — `f_checksum(1003)` đọc word cuối 1000..1003 gồm 3 byte ảnh + 0xFF, khớp `pad`.

- [ ] **Step 3: Thêm vào `run_tests.sh`:** `build test_ota "$N/mb_port/mb_slave_regs.c" "$N/mb_port/mb_ota.c"`. Chạy → Expected: lỗi link (`mb_ota_*` chưa có).

- [ ] **Step 4: Viết `mb_ota.c`**

```c
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
```

- [ ] **Step 5: Nối vào `mb_slave_regs.c`** — thêm `#include "mb_ota.h"`; đầu mỗi callback:

```c
	if (mb_ota_owns(address, quantity)) {
		return mb_ota_read(address, quantity, registers_out);		/* cb_read_holding */
	}
```
```c
	if (mb_ota_owns(address, 1)) {
		return mb_ota_write(address, 1, &value);					/* cb_write_single */
	}
```
```c
	if (mb_ota_owns(address, quantity)) {
		return mb_ota_write(address, quantity, registers);			/* cb_write_multi */
	}
```
Sửa `run_tests.sh`: test_slave_regs cũng cần `mb_ota.c` khi link (`build test_slave_regs "$N/mb_port/mb_slave_regs.c" "$N/mb_port/mb_ota.c"`), và `test_slave_regs.c` gọi `mb_ota_init(NULL)` không cần — `mb_ota_owns` không dùng ops; giữ nguyên.

- [ ] **Step 6: Chạy** `bash tests_host/modbus/run_tests.sh` → Expected: `test_ota: OK` và `test_slave_regs: OK`. Ca nào FAIL → sửa `mb_ota.c`, không nới test.

- [ ] **Step 7: Commit** — `feat(ota): may trang thai OTA qua thanh ghi Modbus 0xF000 + test host`.

---

### Task 8: Nối OTA vào firmware slave, v1.3.0

**Files:** Modify `sources/application/app/app_modbus.cpp`, `CHANGELOG.md`, `README.md`, `platformio.ini` (APP_VERSION).

- [ ] **Step 1: `app_modbus.cpp`** — trong nhánh `#if defined (TASK_MBSLAVE_EN)` thêm include `"mb_ota.h"`, `"task_fw.h"`, `"sys_boot.h"` và:

```cpp
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
```
Trong `app_modbus_init()` nhánh slave, trước `nmbs_server_create`: `mb_ota_init(&ota_ops);`. Thêm kiểm biên dịch cạnh đó:

```cpp
static_assert(MB_OTA_PSK == FIRMWARE_PSK, "MB_OTA_PSK phai bang FIRMWARE_PSK");
```

- [ ] **Step 2: Build** `pio run -e app -e app_mbslave -e boot` → 3 × SUCCESS. Ghi lại Flash của `app_mbslave`.

- [ ] **Step 3: Phiên bản + tài liệu** — `APP_VERSION` → `1.3.0` (app và boot); CHANGELOG `1.3.0 — 2026-09-25`: OTA qua Modbus RS485 (env `app_mbslave`), bảng thanh ghi 0xF000, bootloader không đổi; README: mục "Cập nhật firmware qua RS485" gồm bảng thanh ghi và lệnh PC `python -m epcb_applib.ota COMx release\<file>.bin --slave 1 --baud 9600`.

- [ ] **Step 4: Test host lần cuối + commit** — `bash tests_host/modbus/run_tests.sh` OK → `feat(ota): OTA qua Modbus cho env app_mbslave, v1.3.0`.

---

### Task 9: `epcb_applib.ota` — gửi ảnh từ PC, v1.4.0

**Files (repo `D:/OneDrive/05_Shared_Libraries/epcb-applib`):**
- Create: `src/epcb_applib/ota.py`, `tests/test_ota.py`
- Modify: `pyproject.toml` (version 1.4.0), `src/epcb_applib/__init__.py` (dòng liệt kê module + `__version__` nếu có), `README.md`/CHANGELOG nếu repo có mục phiên bản

**Interfaces:**
- Consumes: `ModbusMaster.write_registers/write_register/read_holding`, `ModbusMaster.timeout`, `ModbusError(msg, code=, frame=)`.
- Produces: `pad4(bytes)->bytes`, `fw_checksum(bytes)->int`, `chunk_regs(offset:int, chunk:bytes)->list[int]`, `class ModbusOta(master, slave=1, chunk=128, psk=FIRMWARE_PSK, begin_timeout=5.0)` với `run(image: bytes, on_progress=None) -> int` (trả checksum), `abort()`; hằng `REG_*`, `CMD_*`, `ST_*`, `STATUS_TEXT`; CLI `python -m epcb_applib.ota PORT FILE [--slave N] [--baud B]`.

- [ ] **Step 1: Viết test trước — `tests/test_ota.py`**

```python
# -*- coding: utf-8 -*-
"""OTA qua Modbus: noi ModbusMaster that voi mot slave gia mo phong dung luat
cua mb_ota.c (ak-base-kit-pio) - offset tuan tu, gui lai khoi, checksum."""
import pytest

from epcb_applib import ota
from epcb_applib.modbus import (FC_HOLDING, FC_WRITE_REG, FC_WRITE_REGS,
                                ModbusError, ModbusMaster, crc16)


class SlaveOtaGia:
    """Thay serial.Serial: giai ma request, tra phan hoi nhu firmware."""

    def __init__(self, slave=1, mat_phan_hoi_lan=()):
        self.slave = slave
        self.flash = bytearray(b"\xff" * 200000)
        self.status, self.len, self.cs, self.psk, self.recv = 0, 0, 0, 0, 0
        self.last = None
        self.erased = self.committed = 0
        self.mat = set(mat_phan_hoi_lan)   # so thu tu request bi "mat" phan hoi
        self.n = 0
        self._out = b""

    # -- giao dien serial ma ModbusMaster dung --
    def reset_input_buffer(self):
        self._out = b""

    @property
    def in_waiting(self):
        return len(self._out)

    def read(self, k):
        r, self._out = self._out[:k], self._out[k:]
        return r

    def close(self):
        pass

    def write(self, f):
        f = bytes(f)
        self.n += 1
        resp = self._xu_ly(f)
        self._out = b"" if self.n in self.mat else resp

    # -- mo phong mb_ota.c --
    def _exc(self, fc, code):
        b = bytes([self.slave, fc | 0x80, code])
        return b + crc16(b)

    def _xu_ly(self, f):
        fc, addr = f[1], f[2] << 8 | f[3]
        if fc == FC_HOLDING:
            q = f[4] << 8 | f[5]
            vals = [self._doc(addr + i) for i in range(q)]
            b = bytes([self.slave, fc, 2 * q]) + b"".join(v.to_bytes(2, "big") for v in vals)
            return b + crc16(b)
        if fc == FC_WRITE_REG:
            regs = [f[4] << 8 | f[5]]
        else:
            q = f[4] << 8 | f[5]
            regs = [f[7 + 2 * i] << 8 | f[8 + 2 * i] for i in range(q)]
        code = self._ghi(addr, regs)
        if code:
            return self._exc(fc, code)
        b = f[:6]
        return b + crc16(b)

    def _doc(self, a):
        return {0xF001: self.status, 0xF002: self.len >> 16, 0xF003: self.len & 0xFFFF,
                0xF004: self.cs, 0xF007: self.recv >> 16, 0xF008: self.recv & 0xFFFF}.get(a, 0)

    def _ghi(self, a, regs):
        if a == 0xF000:
            c = regs[0]
            if c == 1:
                if self.psk != ota.FIRMWARE_PSK or not self.len:
                    self.status = 0x8001
                    return 3
                self.flash[:] = b"\xff" * len(self.flash)
                self.erased += 1
                self.recv, self.last, self.status = 0, None, 1
                return 0
            if c == 2:
                if self.status != 1 or self.recv < self.len:
                    return 3
                if ota.fw_checksum(bytes(self.flash[:self.len])) != self.cs:
                    self.status = 0x8003
                    return 3
                self.status = 2
                self.committed += 1
                return 0
            return 3
        if a == 0xF002:
            self.len = regs[0] << 16 | regs[1]
            self.cs = regs[2]
            self.psk = regs[3] << 16 | regs[4]
            return 0
        if a == 0xF010:
            if self.status != 1:
                return 3
            off = regs[0] << 16 | regs[1]
            data = b"".join(r.to_bytes(2, "big") for r in regs[2:])
            n = min(len(data), self.len - off)
            if self.last == (off, n) and off + n == self.recv:
                return 0
            if off != self.recv:
                self.status = 0x8002
                return 4
            self.flash[off:off + n] = data[:n]
            self.recv += n
            self.last = (off, n)
            return 0
        return 2


def _master(slave_gia):
    mb = ModbusMaster(slave=1, timeout=0.05, retries=2)
    mb.ser = slave_gia
    return mb


def test_pad4_va_checksum_khop_vector_firmware():
    assert ota.pad4(bytes(range(10))) == bytes(range(10)) + b"\xff\xff"
    assert ota.pad4(b"\x01\x02\x03\x04") == b"\x01\x02\x03\x04"
    # cung vector voi tests_host/modbus/test_ota.c ca 7
    assert ota.fw_checksum(bytes(range(10))) == 0x0F0C


def test_chunk_regs_byte_cao_truoc_va_dem_le():
    assert ota.chunk_regs(0x12345, b"\x01\x02\x03") == [0x0001, 0x2345, 0x0102, 0x03FF]


def test_gui_du_anh_thanh_cong():
    s = SlaveOtaGia()
    img = bytes((i * 7 + 3) & 0xFF for i in range(1003))
    tien_do = []
    cs = ota.ModbusOta(_master(s)).run(img, on_progress=lambda a, b: tien_do.append((a, b)))
    assert s.committed == 1 and s.erased == 1
    assert bytes(s.flash[:1004]) == ota.pad4(img)
    assert cs == ota.fw_checksum(img)
    assert tien_do[-1] == (1004, 1004)


def test_mat_phan_hoi_khoi_thi_gui_lai_van_dung():
    # request: 1 header, 2 BEGIN, 3 khoi 0, 4 khoi 128 (mat), 5 gui lai khoi 128...
    s = SlaveOtaGia(mat_phan_hoi_lan={4})
    img = bytes(range(256)) * 2
    ota.ModbusOta(_master(s)).run(img)
    assert s.committed == 1
    assert bytes(s.flash[:512]) == img


def test_loi_checksum_bao_ro_ma_status():
    s = SlaveOtaGia()
    o = ota.ModbusOta(_master(s))
    goc = ota.fw_checksum
    try:
        ota.fw_checksum = lambda d: (goc(d) + 1) & 0xFFFF   # PC tinh sai -> slave tu choi
        with pytest.raises(ModbusError) as e:
            o.run(bytes(64))
    finally:
        ota.fw_checksum = goc
    assert "8003" in str(e.value) and s.committed == 0
```

- [ ] **Step 2: Chạy** `python -m pytest tests/test_ota.py -q` → Expected: FAIL `ImportError: cannot import name 'ota'`.

- [ ] **Step 3: Viết `src/epcb_applib/ota.py`**

```python
# -*- coding: utf-8 -*-
"""
epcb_applib.ota - cap nhat firmware qua Modbus RS485.
=====================================================
Ben PC cua OTA trong ak-base-kit-pio (env app_mbslave, file mb_ota.c):
ghi header -> BEGIN -> tung khoi 128 byte -> COMMIT. Thiet bi kiem checksum,
ghi BSF roi reset; bootloader chep anh tu external flash vao vung app.
Dut giua chung: board van chay app cu (vung app chua bi dung toi).

Dung:
    from epcb_applib import ModbusMaster
    from epcb_applib.ota import ModbusOta

    mb = ModbusMaster("COM5", baud=9600, slave=1, retries=2); mb.open()
    ModbusOta(mb).run(open("app.bin", "rb").read(),
                      on_progress=lambda da, tong: print(da, "/", tong))

Dong lenh:
    python -m epcb_applib.ota COM5 release/app.bin --slave 1 --baud 9600
"""
from .modbus import ModbusError

# Bang thanh ghi - khop mb_ota.h
REG_CMD = 0xF000
REG_STATUS = 0xF001
REG_HEADER = 0xF002      # len hi, len lo, checksum, psk hi, psk lo
REG_RECEIVED = 0xF007
REG_CHUNK = 0xF010       # offset hi, offset lo, du lieu...

CMD_BEGIN, CMD_COMMIT, CMD_ABORT = 1, 2, 3
ST_IDLE, ST_RECEIVING, ST_COMMITTED = 0x0000, 0x0001, 0x0002
STATUS_TEXT = {
    0x8001: "Sai header (psk hoặc độ dài bằng 0)",
    0x8002: "Sai thứ tự khối (offset không liền mạch)",
    0x8003: "Sai checksum - ảnh nhận được không khớp",
    0x8004: "Ảnh lớn hơn vùng app (116 KB)",
}

FIRMWARE_PSK = 0x1A2B3C4D   # = FIRMWARE_PSK trong sys_boot.h
MAX_CHUNK = 128


def pad4(data):
    """Dem 0xFF cho du boi 4 byte - giong flash vua xoa ben thiet bi."""
    data = bytes(data)
    return data + b"\xff" * (-len(data) % 4)


def fw_checksum(data):
    """Tong cac word 32-bit little-endian cua anh da dem, lay 16 bit thap.

    Cung thuat toan fw_ext_checksum() trong task_fw.cpp.
    """
    d = pad4(data)
    s = 0
    for i in range(0, len(d), 4):
        s += int.from_bytes(d[i:i + 4], "little")
    return s & 0xFFFF


def chunk_regs(offset, chunk):
    """[offset hi, offset lo, du lieu...] - byte cao truoc, le thi dem 0xFF."""
    chunk = bytes(chunk)
    if len(chunk) % 2:
        chunk += b"\xff"
    regs = [(offset >> 16) & 0xFFFF, offset & 0xFFFF]
    regs += [chunk[i] << 8 | chunk[i + 1] for i in range(0, len(chunk), 2)]
    return regs


class ModbusOta:
    def __init__(self, master, slave=1, chunk=MAX_CHUNK, psk=FIRMWARE_PSK,
                 begin_timeout=5.0):
        if not 2 <= chunk <= MAX_CHUNK or chunk % 2:
            raise ValueError("chunk phai chan, trong 2..%d" % MAX_CHUNK)
        self.master = master
        self.slave = slave
        self.chunk = chunk
        self.psk = psk
        self.begin_timeout = begin_timeout

    def run(self, image, on_progress=None):
        """Gui ca anh. Tra ve checksum. Loi -> ModbusError kem ma STATUS."""
        img = pad4(image)
        n = len(img)
        cs = fw_checksum(img)
        m, s = self.master, self.slave

        m.write_registers(REG_HEADER, [n >> 16, n & 0xFFFF, cs,
                                       self.psk >> 16, self.psk & 0xFFFF], slave=s)
        cu = m.timeout
        m.timeout = max(cu, self.begin_timeout)   # BEGIN xoa 128 KB flash ngoai
        try:
            self._lenh(CMD_BEGIN)
        finally:
            m.timeout = cu

        off = 0
        while off < n:
            phan = img[off:off + self.chunk]
            self._goi(lambda: m.write_registers(REG_CHUNK, chunk_regs(off, phan), slave=s))
            off += len(phan)
            if on_progress:
                on_progress(off, n)

        self._lenh(CMD_COMMIT)
        return cs

    def abort(self):
        self._lenh(CMD_ABORT)

    # ---- noi bo ----
    def _lenh(self, cmd):
        self._goi(lambda: self.master.write_register(REG_CMD, cmd, slave=self.slave))

    def _goi(self, fn):
        try:
            return fn()
        except ModbusError as e:
            if e.code is None:
                raise
            raise ModbusError(self._mo_ta_status(), code=e.code, frame=e.frame)

    def _mo_ta_status(self):
        try:
            st = self.master.read_holding(REG_STATUS, 1, slave=self.slave)[0]
        except ModbusError:
            return "OTA bị từ chối (không đọc được STATUS)"
        return "OTA lỗi %04Xh: %s" % (st, STATUS_TEXT.get(st, "trạng thái %04Xh" % st))


def main(argv=None):
    import argparse
    from .modbus import ModbusMaster

    p = argparse.ArgumentParser(description="Nạp firmware qua Modbus RS485 (ak-base-kit-pio)")
    p.add_argument("port")
    p.add_argument("file")
    p.add_argument("--slave", type=int, default=1)
    p.add_argument("--baud", type=int, default=9600)
    a = p.parse_args(argv)

    with open(a.file, "rb") as f:
        img = f.read()
    mb = ModbusMaster(a.port, baud=a.baud, slave=a.slave, timeout=0.5, retries=2)
    mb.open()
    try:
        cs = ModbusOta(mb, slave=a.slave).run(
            img, on_progress=lambda da, tong: print("\r%6d / %d byte" % (da, tong), end=""))
        print("\nXong - checksum %04Xh, thiết bị sẽ tự khởi động lại." % cs)
    finally:
        mb.close()


if __name__ == "__main__":
    main()
```

Lưu ý: `ModbusMaster.__enter__/open()` cần pyserial — CLI chỉ chạy khi đã cài `[serial]`.

- [ ] **Step 4: Chạy** `python -m pytest tests/test_ota.py tests/test_modbus.py tests/test_modbus_retry.py -q` → Expected: tất cả pass. Nếu `test_mat_phan_hoi...` fail vì `ModbusMaster` gửi lại nhưng slave giả tính số request khác → đọc lại đúng thứ tự request trong test (header=1, BEGIN=2, khối 0=3, khối 128=4) rồi sửa tập `mat_phan_hoi_lan`, không sửa ota.py.

- [ ] **Step 5: Phiên bản** — `pyproject.toml` `version = "1.4.0"`; trong `__init__.py` thêm dòng mô tả module `ota` vào danh sách đầu file (không import sẵn `ota` ở `__init__` để không đổi thời gian nạp); nếu có `__version__` thì đổi thành `"1.4.0"`. Thêm mục README "OTA qua Modbus" 5 dòng (lệnh CLI + trỏ spec).

- [ ] **Step 6: Commit** (repo epcb-applib, danh tính sẵn có của repo)

```bash
git add src/epcb_applib/ota.py tests/test_ota.py pyproject.toml src/epcb_applib/__init__.py README.md
git commit -m "feat(ota): gui firmware qua Modbus RS485 cho ak-base-kit-pio, v1.4.0

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 7 (anh chạy trên board):** nạp `boot` + `app_mbslave` bằng ST-Link; build lại `app_mbslave` với phiên bản khác (ví dụ sửa tạm `APP_VERSION` thành `1.3.1`), rồi:
```bash
python -m epcb_applib.ota COMx release/<file_app_mbslave>.bin --slave 1 --baud 9600
```
Expected: chạy hết tiến độ, board reset, console bootloader in `[BOOT] copy firmware from external flash`, app lên báo phiên bản mới; đọc thanh ghi 0/1 (FC03) ra đúng phiên bản mới.

---

## Tự rà soát (đã làm khi viết)

- Phủ spec: 1A (Task 1–3), gộp Python (Task 4–5), 1B firmware (Task 6–8), 1B PC (Task 9). Kiểm trên board: Task 3 Step 6, Task 9 Step 7.
- Tên thống nhất: `fw_ext_erase/fw_ext_write/fw_ext_checksum/fw_commit_app_later`, `mb_ota_*`, `MB_OTA_REG_*`, `app_mb`, `app_modbus_init/poll`, `task_polling_mbslave`, `AC_TASK_POLLING_MBSLAVE_ID`, `FW_MB_OTA_COMMIT`.
- Spec nói `fw_commit_app(...)` = BSF + reset; kế hoạch tách thành `fw_commit_app_later` (hẹn 200 ms) + case `FW_MB_OTA_COMMIT` (BSF + reset) để phản hồi Modbus kịp ra dây — đúng ý spec mục OTA bước 3.
