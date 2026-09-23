# Lỗi đã biết trong source base

Tổng hợp các lỗi phát hiện khi dựng sản phẩm từ base này. Mỗi mục đã **đối chiếu lại với mã nguồn
của repo** (commit `d4de67c`), không chỉ với bản sao ở dự án con.

| # | Lỗi | Mức | Trạng thái trong base | Phát hiện |
|---|---|---|---|---|
| 1 | `HardwareSerial::flush()` là hàm rỗng | Cao với RS485 | **Chưa sửa** | 22/09/2026 |
| 2 | `HardwareSerial::write()` đánh mất tín hiệu đánh thức TXE, ghi đè khi ring đầy | **Nghiêm trọng** | **Chưa sửa** | 22/09/2026 |
| 3 | `sys_boot_set()` xoá rồi ghi BSF không bảo vệ — reset giữa chừng là board không vào app | **Nghiêm trọng** | **Chưa sửa** | 23/09/2026 |
| 4 | Vector USART2 phụ thuộc cờ; bật cả `TASK_MBMASTER_EN` và `TASK_ZIGBEE_EN` → `FATAL` | Cao | **Chưa sửa** | 22/09/2026 |
| 5 | `Serial2.begin()` nằm trong `#if TASK_ZIGBEE_EN` | Trung bình | **Chưa sửa** | 22/09/2026 |
| 6 | Không tắt được `TASK_MBMASTER_EN` — build hỏng ở `app_modbus_pull.cpp` và `shell.cpp` | Trung bình | **Chưa sửa** | 22/09/2026 |
| 7 | Macro `USART2_TX_PIN` / `USART2_RX_PIN` đảo ngược so với silicon | Thấp (đang vô hại) | **Chưa sửa** | 22/09/2026 |

Cộng thêm các [bẫy không phải lỗi](#bẫy--không-phải-lỗi-nhưng-đã-làm-mất-thời-gian) ở cuối.

Bản sửa cho #1 và #2 **đã chạy trên phần cứng thật** trong dự án
`14_EPCB AC Controller RS485/03_Firmware/ac-ctrl-stm32` (Modbus RTU slave, 2.000 khung liên tục,
0 % lỗi) và đã chép sang LoRa `node-stm32` (commit `07c5336`, nhánh `007-lora-mac-roles`).

---

## 1. `HardwareSerial::flush()` là hàm rỗng

`sources/application/platform/stm32l/arduino/cores/HardwareSerial.cpp:103`

```cpp
void HardwareSerial::flush() {

}
```

**Hậu quả.** Ai gọi `flush()` cũng tưởng đã đợi xong trong khi chưa đợi gì. Vô hại với console
(ghi rồi đi tiếp), nhưng với **RS485 bán song công** thì hạ chân DE lúc ring còn dữ liệu là **cắt
cụt khung**.

**Sửa.** Đợi ring rỗng thật, có chặn thời gian:

```cpp
void HardwareSerial::flush() {
	uint32_t guard = SERIAL_TX_FULL_GUARD;          /* 2000000UL, khai trong HardwareSerial.h */
	while (_tx_buffer_head != _tx_buffer_tail) {
		if (--guard == 0) {
			return;
		}
	}
}
```

> Ring rỗng **chưa** có nghĩa byte cuối đã rời chân TX — nó còn nằm trong thanh ghi dịch. Trước
> khi hạ DE vẫn phải đợi cờ `USART_FLAG_TC`.

---

## 2. `HardwareSerial::write()` đánh mất tín hiệu đánh thức TXE

`sources/application/platform/stm32l/arduino/cores/HardwareSerial.cpp:107`

```cpp
size_t HardwareSerial::write(uint8_t c) {
	bool _flag_trigger_putc = false;
	if (_tx_buffer_head == _tx_buffer_tail) {        // (A) quyết định TRƯỚC khi ghi
		_flag_trigger_putc = true;
	}
	tx_buffer_index_t i = (_tx_buffer_head + 1) % SERIAL_TX_BUFFER_SIZE;
	_tx_buffer[_tx_buffer_head] = c;                 // không kiểm tra ring đầy
	ENTRY_CRITICAL();
	_tx_buffer_head = i;                             // (B)
	EXIT_CRITICAL();
	if (_flag_trigger_putc) {                        // (C)
		_pf_tringger_putc();
	}
	return 1;
}
```

**Hai lỗi:**

1. **Race giữa (A) và (B).** Ring còn một byte → (A) quyết định *không cần* đánh thức. Trước (B),
   ISR TXE rút nốt byte đó rồi **tắt** ngắt TXE. Byte vừa ghi nằm lại trong ring, không ai đánh
   thức. Lần ghi sau thấy ring **không** rỗng nên cũng không đánh thức → **kẹt cứng** cho tới khi
   ring quay vòng và tình cờ `head == tail`.
2. **Không kiểm tra ring đầy** — ghi đè lên byte chưa kịp gửi.

**Triệu chứng đo được.** Hỏng theo cụm **đúng 6 khung** liên tiếp: 256 byte ring ÷ 43 byte mỗi
phản hồi Modbus ≈ 5,95. Master nhận hai byte đầu (`01 04`) rồi im. Khoảng **4 %** khung hỏng trên
2.000 khung.

**Sửa.** Chờ ring có chỗ (có chặn), rồi bật TXE **vô điều kiện, ngay trong** critical section sau
khi đặt `head` — lúc đó ring chắc chắn không rỗng nên ISR không thể rút phải byte cũ:

```cpp
size_t HardwareSerial::write(uint8_t c) {
	tx_buffer_index_t i = (_tx_buffer_head + 1) % SERIAL_TX_BUFFER_SIZE;

	/* Chờ có chặn: gọi write() trong ENTRY_CRITICAL thì ISR không chạy được,
	 * chờ vô hạn là treo máy. Hết chặn thì bỏ byte, trả 0. */
	uint32_t guard = SERIAL_TX_FULL_GUARD;
	while (i == _tx_buffer_tail) {
		if (--guard == 0) {
			return 0;
		}
	}

	_tx_buffer[_tx_buffer_head] = c;

	ENTRY_CRITICAL();
	_tx_buffer_head = i;
	_pf_tringger_putc();
	EXIT_CRITICAL();

	return 1;
}
```

| | trước | sau |
|---|---|---|
| 2.000 khung liên tục | ~4 % hỏng, theo cụm 6 | **0 %** |
| Thời gian đáp @9600 | 64,5 ms | 64,9 ms trung vị, 66,5 ms tối đa |

**Vì sao nằm im lâu vậy.** Console đi qua `xprintf` chứ không qua `Serial*.write()`, và console là
một chiều — không ai quan tâm lúc nào byte cuối rời chip. Lỗi chỉ lộ ra khi có RS485 bán song công.

---

## 3. `sys_boot_set()` không an toàn khi reset giữa chừng

`sources/application/sys/sys_boot.c:22` (bootloader có bản y hệt ở `sources/boot/sys/sys_boot.c:21`)

```c
internal_flash_unlock();
internal_flash_erase_pages_cal((uint32_t)&_start_boot_share_data_flash, sizeof(sys_boot_t));
internal_flash_write_cal((uint32_t)&_start_boot_share_data_flash, (uint8_t*)sys_boot, sizeof(sys_boot_t));
```

**Hậu quả.** Reset **giữa lúc xoá và lúc ghi** (IWDG, sụt nguồn, debugger treo lõi) để lại BSF ở
trạng thái đã xoá. Trên **STM32L1 flash đã xoá đọc ra `0x00000000`** (không phải `0xFF`), nên
`fw_app_cmd.cmd = 0` — không phải `NONE (1)`, `UPDATE_REQ (2)` hay `UPDATE_RES (3)`. Bootloader
(`sources/boot/app/app.cpp:249`) in:

```
[BOOT] unexpected status
[BOOT] uart boot started
```

và **đứng chờ nạp qua UART — app không chạy nữa**, dù app trong flash vẫn nguyên vẹn.

**Khi nào app ghi BSF.** `task_fw` → `FW_CHECKING_REQ` (`app/task_fw.cpp`) chạy mỗi lần khởi động
và ghi lại BSF **nếu có gì khác** — trong đó có `current_fw_app_header`. Tức là **lần boot đầu sau
mỗi lần nạp firmware mới** luôn có một lần xoá-ghi BSF. Giai đoạn phát triển nạp liên tục nên cửa
sổ rủi ro mở rất thường.

**Đã gặp thật (23/09/2026).** Đọc thanh ghi bằng `STM32_Programmer_CLI -c port=SWD mode=HOTPLUG`
trên board đang chạy: `HOTPLUG` **treo lõi** trong khi **IWDG vẫn đếm** → reset trúng
`sys_boot_set()`. Triệu chứng phụ gây nhầm: mọi thanh ghi ngoại vi đọc ra `0` (app chưa chạy nên
chưa bật clock) — dễ tưởng code mới làm hỏng ngoại vi.

**Cứu** — ghi lại lệnh hợp lệ; ô đang ở trạng thái đã xoá nên ghi thẳng được:

```bash
STM32_Programmer_CLI -c port=SWD mode=UR -w32 0x08002044 0x00000001
```

`0x44` = bốn `firmware_header_t` (12 byte mỗi cái) + `fw_boot_cmd` (20 byte) → `fw_app_cmd.cmd`.
Kiểm lại: đọc `0x08002000` 0x50 byte, hai `psk` đầu phải là `0x1A2B3C4D`.

**Hướng sửa (chưa làm).** Ít nhất một trong hai:

- **Bootloader:** coi `cmd == 0` (BSF vừa bị xoá) như `SYS_BOOT_CMD_NONE` **nếu** app hợp lệ (PSK
  và checksum đúng), thay vì rơi vào uart boot.
- **BSF hai bản:** ghi luân phiên hai page, mỗi bản có số thứ tự + CRC; đọc bản mới nhất còn CRC
  đúng. Mất điện ở bất kỳ thời điểm nào vẫn còn một bản tốt.

---

## 4. Vector USART2 phụ thuộc cờ biên dịch

`sources/application/platform/stm32l/system.c:172`

```c
#if defined (TASK_MBMASTER_EN) && defined (TASK_ZIGBEE_EN)
    default_handler,        //  USART2
#elif defined (TASK_MBMASTER_EN)
    vMBPUSART2ISR,          //  USART2
#elif defined (TASK_ZIGBEE_EN)
    uart2_irq,              //  USART2
#else
    default_handler,        //  USART2
#endif
```

**Hậu quả** (suy từ mã nguồn — tổ hợp hai cờ chưa chạy thử trên board):

- Bật **cả hai** cờ: `Serial2.begin()` (của Zigbee) bật ngắt RXNE, nhưng vector là
  `default_handler` → `FATAL("SY", 0xEE)` ngay **byte đầu tiên** nhận về.
- Dự án muốn dùng `Serial2` cho việc khác (Modbus slave, cầu UART…) mà không bật Zigbee: vector
  cũng là `default_handler`.

Build vẫn xanh, mọi ký hiệu có trong ELF — lỗi chỉ lộ khi chạy.

**Cách phát hiện.** Đọc thẳng ô vector trong `.bin` rồi đối chiếu bảng ký hiệu (vector 38 = mục
thứ 54 tính từ đầu bảng):

```bash
python -c "import struct;d=open('firmware.bin','rb').read();print(hex(struct.unpack('<I',d[54*4:54*4+4])[0]))"
arm-none-eabi-nm firmware.elf | grep -i usart2
```

**Hướng sửa.** Tách "ai dùng USART2" thành **một** cờ riêng (ví dụ `USART2_OWNER_SERIAL2` /
`USART2_OWNER_MBMASTER`) và `#error` khi hai chủ cùng bật. Ô vector phải trỏ `uart2_irq`, **không**
trỏ thẳng `sys_irq_uart2`: `uart2_irq` bọc `task_entry_interrupt()` / `task_exit_interrupt()` mà
kernel AK dùng để đếm độ sâu ngắt.

---

## 5. `Serial2.begin()` nằm trong `#if TASK_ZIGBEE_EN`

`sources/application/app/app.cpp:246`

```cpp
#if defined (TASK_ZIGBEE_EN)
	Serial2.begin();
	Serial2.setTimeout(100);
#endif
```

**Hậu quả.** Dùng `Serial2` cho việc khác mà không bật Zigbee → USART2 **chưa từng được mở**, không
nhận được byte nào. Cùng gốc với #4: USART2 bị gắn cứng vào Zigbee.

Thêm: `HardwareSerial::begin()` **không có tham số baud** — `io_uart2_cfg()`
(`platform/stm32l/io_cfg.c:663`) cố định **115200**. Muốn baud khác (Modbus thường 9600) phải gọi
`USART_Init()` lại sau `begin()`. `USART_Init()` không động tới `RXNEIE`, nên ngắt thu vẫn bật —
đã xác nhận bằng cách đọc `USART2->CR1` trên chip.

---

## 6. Không tắt được `TASK_MBMASTER_EN`

`sources/application/app/app_modbus_pull.cpp:107` · `sources/application/app/shell.cpp:34, 85, 111, 944`

Cờ trông như công tắc bật/tắt module, nhưng **tắt đi là build hỏng**. Đã thử trên chính repo này
(bỏ `-DTASK_MBMASTER_EN` trong `[env:app]`, `pio run -e app`):

```
app_modbus_pull.cpp:107:30: error: 'xMBMMaster' was not declared in this scope
```

Nguyên nhân: `xMBMMaster` chỉ được khai trong `#if defined(TASK_MBMASTER_EN)` (`app_data.h:53`),
nhưng `app_modbus_pull.cpp` **luôn** được biên dịch (`build_src_filter` lấy `app/*.cpp`) và dùng
nó vô điều kiện. Qua được chỗ đó thì `shell.cpp` cũng `#include "app_modbus_pull.h"`, khai báo
`shell_modbus`, đưa vào bảng lệnh, và gọi `updateDataModbusDevice()` — **tất cả vô điều kiện**.

Dự án không cần Modbus master — hoặc làm Modbus **slave**, vì mbmaster và slave tranh cùng USART2
và TIM4 — buộc phải tắt cờ này.

**Hướng sửa.** Bọc toàn bộ `app_modbus_pull.cpp` trong `#if defined(TASK_MBMASTER_EN)`, và trong
`shell.cpp` bọc cả `#include`, khai báo, dòng bảng lệnh lẫn thân `shell_modbus`. Bọc thiếu một
chỗ thì lỗi chuyển từ bước biên dịch sang bước **link** — khó lần hơn nhiều.

> Tắt mbmaster trả lại **~7 KB flash** và ~470 byte stack — đáng kể trên chip 128K/16K.

---

## 7. Macro chân USART2 đảo ngược so với silicon

`sources/application/platform/stm32l/io_cfg.h:118, 124`

```c
#define USART2_TX_PIN    GPIO_Pin_3     // silicon: PA3 là USART2_RX
#define USART2_RX_PIN    GPIO_Pin_2     // silicon: PA2 là USART2_TX
```

**Đang vô hại** vì cả hai chân cùng được đặt AF7 (trên L1 `AF_USART1 == AF_USART2 == AF7`), không
có chỗ nào cấu hình riêng từng chân theo tên. Nhưng **đừng tin tên macro khi vẽ board mới** —
đi theo datasheet: **PA2 = TX, PA3 = RX**, khớp net `RS485_TX` / `RS485_RX` trên AK MCU KIT 3I0.

---

## Bẫy — không phải lỗi, nhưng đã làm mất thời gian

| Bẫy | Chi tiết |
|---|---|
| **`argv` của lệnh shell là cả dòng** | `shell_xxx(uint8_t* argv)` nhận nguyên dòng lệnh, kể cả tên lệnh. Base tự đếm offset (`*(argv + 4)` trong `shell_dbg`, `*(argv + 7)` trong `shell_modbus`). Lệnh mới phải bỏ token đầu |
| **Bản đồ flash** | Base này là chip **128K**: App ở **`0x08003000`**. Số `0x08009000` là của bản 256K — link sai chỗ thì board treo im lặng sau `jump_to_application()` |
| **SWD `mode=HOTPLUG` trên board đang chạy** | Không vô hại — xem #3. Để board tự báo thanh ghi qua console; nếu bắt buộc đọc SWD thì gộp vào **một** lần gọi CLI |
| **`TIM_ClearITPendingBit(TIMx, TIM_IT_CC1)` không xoá `CC1OF`** | Của thư viện SPL, không phải của base. Dùng input capture thì xoá bằng `TIMx->SR = ~(TIM_SR_CC1IF \| TIM_SR_CC1OF \| TIM_SR_UIF)` trước khi bật — cờ sót làm lệch pha cả lần bắt sau |

---

*EPCB Vietnam · contact@epcb.vn · www.epcb.vn*
