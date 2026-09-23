# Lỗi đã biết trong source base

Các lỗi phát hiện khi dựng sản phẩm từ base này. **Tất cả đã sửa trong `v1.1.0`** — dự án mới
clone từ tag `v1.1.0` trở đi không còn mang chúng. Dự án tạo từ `v1.0.0` thì phải tự chép bản sửa
sang (xem dòng *File* của từng mục).

| # | Lỗi | Mức | Trạng thái | Kiểm chứng |
|---|---|---|---|---|
| 1 | `HardwareSerial::flush()` là hàm rỗng | Cao với RS485 | ✅ Đã sửa v1.1.0 | Phần cứng |
| 2 | `HardwareSerial::write()` đánh mất tín hiệu đánh thức TXE, ghi đè khi ring đầy | **Nghiêm trọng** | ✅ Đã sửa v1.1.0 | Phần cứng, 2.000 khung |
| 3 | Bootloader kẹt ở "uart boot" dù app còn nguyên — BSF bị xoá giữa chừng, hoặc reset trong 5 s sau OTA | **Nghiêm trọng** | ✅ Đã sửa v1.1.0 (boot 0.0.2) | Phần cứng, 6 kịch bản |
| 4 | Vector USART2 phụ thuộc cờ; bật cả `TASK_MBMASTER_EN` và `TASK_ZIGBEE_EN` → `FATAL` | Cao | ✅ Đã sửa v1.1.0 | Build 4 tổ hợp cờ |
| 5 | `Serial2.begin()` nằm trong `#if TASK_ZIGBEE_EN`; baud cố định 115200 | Trung bình | ✅ Đã sửa v1.1.0 | Build |
| 6 | Không tắt được `TASK_MBMASTER_EN` — build hỏng | Trung bình | ✅ Đã sửa v1.1.0 | Build |
| 7 | Macro chân USART2 và RS485 đảo TX/RX so với silicon | Thấp | ✅ Đã sửa v1.1.0 | Build |

Cộng thêm các [bẫy không phải lỗi](#bẫy--không-phải-lỗi-nhưng-đã-làm-mất-thời-gian) ở cuối.

**Nâng board đang chạy lên bản sửa:**

| Lỗi | Cần nạp lại |
|---|---|
| #1, #2, #4–#7 | **app** — là code của app |
| #3 | **bootloader** — nạp `release/boot/ak_base_kit_boot_v1.1.0.bin` tại `0x08000000`. Console in `[BOOT] version: 0.0.2` là đã lên bản mới |

---

## 1. `HardwareSerial::flush()` là hàm rỗng

**File:** `sources/application/platform/stm32l/arduino/cores/HardwareSerial.cpp`

```cpp
void HardwareSerial::flush() {

}
```

Ai gọi `flush()` cũng tưởng đã đợi xong trong khi chưa đợi gì. Vô hại với console (ghi rồi đi
tiếp), nhưng với **RS485 bán song công** thì hạ chân DE lúc ring còn dữ liệu là **cắt cụt khung**.

**Đã sửa:** đợi ring rỗng thật, có chặn số vòng (`SERIAL_TX_FULL_GUARD` trong `HardwareSerial.h`)
để không treo nếu bị gọi lúc ngắt đang tắt.

> Ring rỗng **chưa** có nghĩa byte cuối đã rời chân TX — nó còn trong thanh ghi dịch. Trước khi
> hạ DE vẫn phải đợi cờ `USART_FLAG_TC`.

---

## 2. `HardwareSerial::write()` đánh mất tín hiệu đánh thức TXE

**File:** `sources/application/platform/stm32l/arduino/cores/HardwareSerial.cpp`

Bản cũ:

```cpp
if (_tx_buffer_head == _tx_buffer_tail) {        // (A) quyết định TRƯỚC khi ghi
	_flag_trigger_putc = true;
}
_tx_buffer[_tx_buffer_head] = c;                 // không kiểm tra ring đầy
ENTRY_CRITICAL();
_tx_buffer_head = i;                             // (B)
EXIT_CRITICAL();
if (_flag_trigger_putc) {                        // (C)
	_pf_tringger_putc();
}
```

1. **Race giữa (A) và (B).** Ring còn một byte → (A) quyết định *không cần* đánh thức. Trước (B),
   ISR TXE rút nốt byte đó rồi **tắt** ngắt TXE. Byte vừa ghi nằm lại, không ai đánh thức. Lần ghi
   sau thấy ring **không** rỗng nên cũng không đánh thức → **kẹt cứng** tới khi ring quay vòng.
2. **Không kiểm tra ring đầy** — ghi đè lên byte chưa kịp gửi.

**Triệu chứng đo được:** hỏng theo cụm **đúng 6 khung** liên tiếp (256 byte ring ÷ 43 byte mỗi
phản hồi Modbus ≈ 5,95); master nhận hai byte đầu rồi im; ~4 % khung hỏng.

**Đã sửa:** chờ ring có chỗ (có chặn; hết chặn thì bỏ byte và trả 0 thay vì treo), rồi bật TXE
**vô điều kiện, ngay trong** critical section sau khi đặt `head`:

```cpp
ENTRY_CRITICAL();
_tx_buffer_head = i;
_pf_tringger_putc();
EXIT_CRITICAL();
```

| | trước | sau |
|---|---|---|
| 2.000 khung Modbus liên tục @9600 | ~4 % hỏng, theo cụm 6 | **0 %** |
| Thời gian đáp | 64,5 ms | 64,9 ms trung vị, 66,5 ms tối đa |

Đo trên AK MCU KIT 3I0 trong dự án `14_EPCB AC Controller RS485`, 22/09/2026.

---

## 3. Bootloader kẹt ở "uart boot" dù app còn nguyên

**File:** `sources/boot/app/app.cpp` · nguyên nhân gốc ở `sys_boot_set()`
(`sources/application/sys/sys_boot.c`, `sources/boot/sys/sys_boot.c`)

`sys_boot_set()` **xoá rồi mới ghi** BSF. Trên **STM32L1 flash đã xoá đọc ra `0x00000000`**, nên
reset lọt vào giữa (IWDG, sụt nguồn, debugger treo lõi) để lại `fw_app_cmd.cmd = 0`. Bootloader
cũ chỉ chạy app khi `cmd == NONE`, còn lại rơi vào:

```
[BOOT] unexpected status
[BOOT] uart boot started
```

và **đứng chờ nạp UART mãi mãi**, nhìn từ ngoài như board treo.

Cùng một kiểu kẹt, ba đường dẫn tới:

| Đường dẫn | Khi nào |
|---|---|
| BSF bị xoá giữa chừng | App ghi lại BSF ở **lần boot đầu sau mỗi lần nạp firmware mới** (`task_fw` → `FW_CHECKING_REQ`), nên giai đoạn phát triển cửa sổ rủi ro mở rất thường. Gặp thật 23/09/2026: đọc thanh ghi bằng SWD `mode=HOTPLUG` treo lõi trong khi IWDG vẫn đếm |
| Reset trong **5 s sau OTA** | Bootloader ghi `cmd = UPDATE_RES` rồi chạy app; app đợi `FW_UPDATE_REQ_INTERVAL` (5 s) mới xoá về `NONE`. Reset trong khoảng đó → `UPDATE_RES` cũng không phải `NONE` |
| Board mới nạp app bằng SWD | BSF chưa ai ghi — lý do tồn tại target `pio run -e app -t bsf` |

Kèm một lỗi ngược chiều: BSF ghi "chạy app" nhưng **flash app trống** (xoá bằng SWD, nạp dở) thì
bootloader cũ **vẫn nhảy vào** — HardFault, reset vòng tròn — thay vì chờ nạp UART.

**Đã sửa (bootloader 0.0.2):** bootloader tự kiểm **bảng vector của app** thay vì chỉ tin BSF:

- con trỏ stack ban đầu nằm trong SRAM, căn 4 byte;
- vector reset là địa chỉ Thumb (bit 0 = 1) nằm trong vùng app.

Flash đã xoá đọc ra 0 → trượt cả hai. Hàm `app_image_is_plausible()`.

1. BSF không nói "chạy app" **nhưng** ảnh app hợp lệ → vá những trường BSF hỏng (lệnh ngoài
   `NONE..UPDATE_RES` về `NONE`, `psk` sai về `FIRMWARE_PSK`) rồi chạy app. Lệnh hợp lệ thì **giữ
   nguyên** — `UPDATE_RES` vẫn còn để app gửi thông báo "cập nhật xong". Header app (`bin_len`,
   `checksum`) app tự điền lại ở `FW_CHECKING_REQ`.
2. BSF nói "chạy app" **nhưng** ảnh app không hợp lệ → rơi xuống "unexpected status" → chờ nạp UART.
3. Luồng cập nhật từ external flash **không đổi**.

Chỉ đổi bootloader, **không đổi định dạng BSF** — app cũ lẫn app mới chạy được với bootloader mới,
và board ngoài hiện trường chỉ OTA app vẫn an toàn. Bootloader thêm 268 byte (6.820 / 8.192).

Đã chọn cách này thay vì BSF hai bản có CRC: cách đó an toàn hơn trước mất điện nhưng đổi định dạng
BSF, nên board chỉ được OTA app mới trong khi bootloader vẫn cũ sẽ **hỏng boot**.

**Kiểm chứng trên AK MCU KIT 3I0, 23/09/2026:**

| Kịch bản | Bootloader cũ (0.0.1) | Bootloader mới (0.0.2) |
|---|---|---|
| BSF bị xoá (`cmd 0`), app còn | kẹt uart boot | vá BSF → chạy app |
| `cmd = UPDATE_RES`, app còn | kẹt uart boot | chạy app, giữ `UPDATE_RES` |
| BSF bình thường | chạy app | chạy app, **không ghi flash** |
| BSF hợp lệ, **app trống** | nhảy vào rác | uart boot |
| BSF bị xoá, **app trống** | uart boot | uart boot |
| Nạp app bằng SWD lên BSF trống (board mới) | kẹt uart boot | vá BSF → chạy app |

Sau khi vá, app tự điền lại header vào BSF (`len 62176, checksum 0x61BE` — khớp với lúc trước khi
xoá).

**Cứu board còn mang bootloader cũ** — ghi lại lệnh hợp lệ; ô đang ở trạng thái đã xoá nên ghi
thẳng được:

```bash
STM32_Programmer_CLI -c port=SWD mode=UR -w32 0x08002044 0x00000001
```

`0x44` = bốn `firmware_header_t` (12 byte) + `fw_boot_cmd` (20 byte) → `fw_app_cmd.cmd`. Nếu cả
`psk` cũng mất thì chạy `pio run -e app -t bsf`.

---

## 4. Vector USART2 phụ thuộc cờ biên dịch

**File:** `sources/application/platform/stm32l/system.c`, `sources/application/app/app.h`

Bản cũ:

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

Bật cả hai cờ: `Serial2.begin()` bật ngắt RXNE nhưng vector là `default_handler` →
`FATAL("SY", 0xEE)` ở byte đầu tiên nhận về. Dùng `Serial2` cho việc khác mà không bật Zigbee:
vector cũng là `default_handler`. Build vẫn xanh.

**Đã sửa:** USART2 có **một chủ duy nhất**, khai trong `app.h`:

| Cờ | Chủ của USART2 | Vector |
|---|---|---|
| `TASK_MBMASTER_EN` | mbmaster | `vMBPUSART2ISR` |
| `SERIAL2_EN` | Arduino `Serial2` | `uart2_irq` |
| không cờ nào | — | `default_handler` |

- `TASK_ZIGBEE_EN` **tự bật** `SERIAL2_EN` (Zigbee nói chuyện qua `Serial2`).
- Bật `TASK_MBMASTER_EN` cùng `SERIAL2_EN` (hoặc Zigbee) → **`#error` lúc biên dịch**, thông báo
  rõ ràng, thay vì `FATAL` lúc chạy.
- Vector trỏ `uart2_irq`, **không** trỏ thẳng `sys_irq_uart2`: `uart2_irq` bọc
  `task_entry_interrupt()` / `task_exit_interrupt()` mà kernel AK dùng để đếm độ sâu ngắt.

Kiểm bằng cách đọc thẳng ô vector 38 (mục thứ 54) trong `firmware.bin` rồi đối chiếu `nm`:

| Tổ hợp cờ | Build | Vector USART2 |
|---|---|---|
| Mặc định (`TASK_MBMASTER_EN`) | OK | `vMBPUSART2ISR` |
| Tắt mbmaster | OK | `default_handler` |
| Tắt mbmaster, `SERIAL2_EN` | OK | `uart2_irq` |
| `TASK_MBMASTER_EN` + `SERIAL2_EN` | `#error` | — |

```bash
python -c "import struct;d=open('firmware.bin','rb').read();print(hex(struct.unpack('<I',d[54*4:54*4+4])[0]))"
arm-none-eabi-nm firmware.elf | grep -i usart2
```

---

## 5. `Serial2.begin()` nằm trong `#if TASK_ZIGBEE_EN`; baud cố định

**File:** `sources/application/app/app.cpp`, `sources/application/platform/stm32l/io_cfg.h/.c`

Dùng `Serial2` cho việc khác mà không bật Zigbee thì USART2 **chưa từng được mở**. Thêm nữa,
`HardwareSerial::begin()` không có tham số baud và `io_uart2_cfg()` cố định **115200**.

**Đã sửa:**

- `Serial2.begin()` chạy theo `SERIAL2_EN` (cùng cờ quyết định vector ở #4).
- Baud lấy từ `SERIAL2_BAUDRATE`, mặc định 115200. Ví dụ Modbus:

```ini
    -DSERIAL2_EN
    -DSERIAL2_BAUDRATE=9600
```

---

## 6. Không tắt được `TASK_MBMASTER_EN`

**File:** `sources/application/app/app_modbus_pull.cpp`, `sources/application/app/shell.cpp`

Cờ trông như công tắc bật/tắt module, nhưng tắt đi là build hỏng:

```
app_modbus_pull.cpp:107:30: error: 'xMBMMaster' was not declared in this scope
```

`xMBMMaster` chỉ được khai khi bật cờ (`app_data.h`), nhưng `app_modbus_pull.cpp` luôn được biên
dịch và dùng nó vô điều kiện. Qua được chỗ đó thì `shell.cpp` cũng include, khai báo, đưa vào bảng
lệnh và gọi code Modbus — tất cả vô điều kiện.

Dự án không cần Modbus master — hoặc làm Modbus **slave**, vì mbmaster và slave tranh cùng USART2
và TIM4 — buộc phải tắt cờ này.

**Đã sửa:** bọc cả file `app_modbus_pull.cpp`, và trong `shell.cpp` bọc include, khai báo, dòng
bảng lệnh lẫn thân `shell_modbus`. Tắt mbmaster trả lại **7,8 KB flash** và 752 byte RAM
(58.180 → 50.384 byte flash).

---

## 7. Macro chân USART2 / RS485 đảo TX/RX so với silicon

**File:** `sources/application/platform/stm32l/io_cfg.h`

Theo datasheet STM32L151: **PA2 = USART2_TX, PA3 = USART2_RX** (AF7), khớp net `RS485_TX` /
`RS485_RX` trên AK MCU KIT 3I0. Bản cũ đặt ngược ở **hai** bộ macro, kèm sai tên AF:

```c
#define USART2_TX_PIN        GPIO_Pin_3        // thật ra là RX
#define USART2_TX_AF         GPIO_AF_USART1    // đây là USART2
#define USART2_RX_PIN        GPIO_Pin_2        // thật ra là TX
#define USART_RS485_TX_PIN   (GPIO_Pin_3)      // cùng lỗi
#define USART_RS485_RX_PIN   (GPIO_Pin_2)
```

Trước giờ **vô hại**: hai chân được cấu hình giống hệt nhau (AF, push-pull, kéo lên), và trên L1
`AF_USART1 == AF_USART2 == 7` — ở chế độ AF, chính USART quyết định chân nào phát. Nhưng sẽ thành
lỗi thật khi ai đó cấu hình riêng từng chân theo tên (TX open-drain, tạm chuyển TX sang GPIO để gửi
break), hoặc tra tên macro khi vẽ board mới.

**Đã sửa:** đổi lại `PIN`, `SOURCE` và `AF` cho cả `USART2_*` lẫn `USART_RS485_*`.

---

## Bẫy — không phải lỗi, nhưng đã làm mất thời gian

| Bẫy | Chi tiết |
|---|---|
| **`argv` của lệnh shell là cả dòng** | `shell_xxx(uint8_t* argv)` nhận nguyên dòng lệnh, kể cả tên lệnh. Base tự đếm offset (`*(argv + 4)` trong `shell_dbg`, `*(argv + 7)` trong `shell_modbus`). Lệnh mới phải bỏ token đầu |
| **Bản đồ flash** | Base này là chip **128K**: App ở **`0x08003000`**. Số `0x08009000` là của bản 256K — link sai chỗ thì board treo im lặng sau `jump_to_application()` |
| **SWD `mode=HOTPLUG` trên board đang chạy** | Treo lõi trong khi IWDG vẫn đếm → có thể reset giữa lúc ghi BSF (#3). Để board tự báo thanh ghi qua console; nếu bắt buộc đọc SWD thì gộp vào **một** lần gọi CLI |
| **`TIM_ClearITPendingBit(TIMx, TIM_IT_CC1)` không xoá `CC1OF`** | Của thư viện SPL, không phải của base. Dùng input capture thì xoá bằng `TIMx->SR = ~(TIM_SR_CC1IF \| TIM_SR_CC1OF \| TIM_SR_UIF)` trước khi bật — cờ sót làm lệch pha cả lần bắt sau |

---

*EPCB Vietnam · contact@epcb.vn · www.epcb.vn*
