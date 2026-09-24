# Thay mbmaster bằng nanoMODBUS + OTA qua Modbus RS485

Ngày: 25/09/2026 · Trạng thái: đã duyệt (Hoàng Anh) · Áp dụng: ak-base-kit-pio (STM32L151CBT6)

## Vì sao

1. `networks/mbmaster-v2.9.6` là bản Modbus master **thương mại** của embedded-solutions.at
   ("Copyright … All rights reserved", không có file license). Mọi dự án sinh từ source
   base đang mang theo nó → rủi ro bản quyền.
2. mbmaster chỉ có master. Muốn cập nhật firmware qua đường RS485 sẵn có thì thiết bị phải
   làm được slave.

nanoMODBUS (github.com/debevv/nanoMODBUS, MIT, 1 cặp `.c/.h`, không cấp phát động) làm
được cả client (master) lẫn server (slave).

## Phần 1A — thay thư viện

### Cấu trúc
- `sources/application/networks/nanomodbus/` : `nanomodbus.c`, `nanomodbus.h`, `LICENSE`
  (bản gốc, không sửa).
- `sources/application/networks/mb_port/rs485_port.{c,h}` : lớp port cho USART2.
  - RX: ISR RXNE đẩy byte vào ring buffer 256 byte.
  - `read(buf, count, timeout_ms)`: lấy từ ring buffer, chờ theo `sys_ctrl_millis()`;
    timeout < 0 = chờ mãi, 0 = không chờ.
  - `write(buf, count, timeout_ms)`: `io_rs485_dir_high()`, gửi polling TXE, chờ TC,
    `io_rs485_dir_low()`.
  - Gọi `sys_ctrl_independent_watchdog_reset()` trong vòng chờ.
  - Không dùng TIM4 nữa (nanoMODBUS tự đo khoảng lặng bằng byte timeout).
- `sources/application/app/app_modbus.{cpp,h}` : khởi tạo client/server, giữ handle.

### Chế độ
- `TASK_MBMASTER_EN` (mặc định bật): client nanoMODBUS. API `appMBMasterRead/Write`
  trong `app_modbus_pull.cpp` giữ tên và tham số, thân hàm đổi sang `nmbs_*`, kiểu trả về
  đổi từ `eMBErrorCode` sang `nmbs_error` của nanoMODBUS (so với `NMBS_ERROR_NONE`).
  `appMBMasterWrite` vốn nằm trong `#if 0` (code chết, gọi API cũ) → xóa.
  Lời gọi vẫn chặn như cũ (≈100 ms/lần ở 9600 baud).
- `TASK_MBSLAVE_EN` (mới, mặc định tắt): server nanoMODBUS, địa chỉ slave mặc định 1.
  `nmbs_server_poll()` gọi từ `task_polling_run` với read timeout 0 → không chặn khi
  đường truyền rảnh. Có bảng thanh ghi demo: 0 = phiên bản app (major<<8|minor),
  1 = patch, 2 = uptime giây (u16).
- `app.h`: `#error` nếu bật cả `TASK_MBMASTER_EN` và `TASK_MBSLAVE_EN` (cùng USART2),
  giữ `#error` cũ với `SERIAL2_EN`.
- Xóa `networks/mbmaster-v2.9.6`, include path và filter tương ứng trong `platformio.ini`
  (+ `Makefile.mk` nếu còn tham chiếu), xóa `vMBPUSART2ISR` khỏi bảng vector; ISR USART2
  mới là `rs485_port_irq()` trong `rs485_port.c`, gọi qua vỏ `rs485_irq()` ở `system.c` (bọc task_entry/exit_interrupt như `uart2_irq`).

### Kiểm tra
- `pio run -e app` và `pio run -e boot` build sạch; ghi lại kích thước flash trước/sau.
- Build thêm biến thể `TASK_MBSLAVE_EN` (env `app_mbslave`) để chắc chế độ slave biên dịch.
- Test host (gcc trên PC, `tests_host/modbus/` — không đặt trong `test/` để `pio test` không nhặt nhầm): client + server nanoMODBUS nối qua
  UART giả (hai ring buffer chéo nhau) — đọc/ghi thanh ghi, timeout, CRC sai.
- Trên board AK thật: `modbus r` đọc ES35-SW ra nhiệt độ/độ ẩm hợp lý (cần anh chạy).

## Phần 1B — OTA qua Modbus (cần `TASK_MBSLAVE_EN`)

Tận dụng nguyên cơ chế hiện có: ảnh mới ghi vào external flash `APP_FLASH_FIRMWARE_START_ADDR`
(0x80000, 2 block 64K), kiểm checksum (tổng word 32-bit little-endian, lấy 16 bit thấp;
PC đệm ảnh bằng 0xFF cho đủ bội 4 byte trước khi gửi — giống flash đã xóa),
đặt BSF `fw_app_cmd = UPDATE_REQ / EXTERNAL_FLASH`, reset → bootloader chép.
**Bootloader không sửa.**

### Tách hàm dùng chung khỏi task_fw.cpp
`fw_ext_erase()`, `fw_ext_write(off, data, len)`, `fw_ext_checksum(len)`,
`fw_commit_app(const firmware_header_t*)` (= `fw_update_app_req_c_external_flash_io_none`
+ reset). task_fw gọi lại các hàm này — hành vi đường UART cũ không đổi.

### Bảng thanh ghi OTA (holding, base 0xF000)
| Địa chỉ | R/W | Nội dung |
|---|---|---|
| 0xF000 | W | CMD: 1 = BEGIN, 2 = COMMIT, 3 = ABORT |
| 0xF001 | R | STATUS: 0 idle, 1 đang nhận, 2 đã commit (sắp reset), 0x8001 sai header, 0x8002 sai offset, 0x8003 sai checksum, 0x8004 quá cỡ |
| 0xF002–0xF003 | R/W | bin_len (hi, lo) |
| 0xF004 | R/W | checksum |
| 0xF005–0xF006 | R/W | psk (hi, lo) |
| 0xF007–0xF008 | R | số byte đã nhận (hi, lo) |
| 0xF010–0xF011 | W | offset khối (hi, lo) |
| 0xF012–0xF051 | W | dữ liệu khối, tối đa 64 thanh ghi = 128 byte, byte cao trước |

Luồng (PC là master):
1. FC16 ghi header 0xF002..0xF006, rồi FC06 CMD=BEGIN. Slave kiểm psk = `FIRMWARE_PSK`,
   bin_len ≤ 116K **và bin_len phải là bội số của 4** (PC luôn đệm ảnh bằng 0xFF cho đủ bội 4
   byte rồi mới tính checksum và gửi bin_len đã đệm — bin_len không phải bội 4 bị từ chối
   ngay bằng STATUS 0x8001, exception 0x03, không xóa flash); nếu qua hết thì xóa 2 block
   external flash (chặn ~1–2 s → PC dùng timeout 5 s cho lệnh này).
2. Mỗi khối: một FC16 bắt đầu 0xF010 gồm offset + dữ liệu. Offset phải bằng số byte đã nhận;
   nếu bằng offset khối trước (PC gửi lại vì mất phản hồi) thì trả OK mà không ghi lại;
   khác nữa (kể cả offset ≥ bin_len — coi là sai offset, không phải quá cỡ) → exception 0x04
   và STATUS 0x8002. Sau 0x8002, phiên coi như hỏng: phải bắt đầu lại từ FC06 CMD=BEGIN.
   Khối cuối có thể ngắn hơn 128 byte.
3. FC06 CMD=COMMIT: tính checksum trên đúng bin_len (bin_len luôn là bội 4 — xem điều kiện
   BEGIN ở trên, nên không cần làm tròn). Đúng → STATUS 2, trả phản hồi, hẹn AK timer 200 ms
   rồi `fw_commit_app()`. Sai → STATUS 0x8003, app cũ chạy tiếp.
   Sau khi đã COMMIT (STATUS 2), thiết bị đang chờ AK timer 200 ms rồi tự reset để bootloader
   chép ảnh — tại đây bootloader sẽ xoá internal flash và chép từ external flash TRƯỚC KHI
   kiểm checksum, nên state machine phải khoá lại: BEGIN, ABORT và mọi ghi header
   (0xF002..0xF006) đều bị từ chối bằng exception 0x03, STATUS giữ nguyên 2 — chỉ có COMMIT
   gửi lại (PC mất phản hồi lần đầu) là được chấp nhận, trả OK mà không gọi lại thao tác
   commit vật lý lần hai.
4. Modbus broadcast (unit_id 0, FC06/FC16 gửi tới địa chỉ 0): các thanh ghi OTA hoàn toàn bỏ
   qua broadcast — trả exception 0x01 (ILLEGAL_FUNCTION) nội bộ và KHÔNG chạm tới state
   machine OTA (không BEGIN/COMMIT/ghi khối/ghi header). Vì broadcast không có phản hồi nên
   phía PC sẽ không nhận ra lỗi này — OTA không bao giờ được gửi qua địa chỉ broadcast.
5. Mất điện/đứt giữa chừng trước COMMIT: internal flash chưa bị đụng → board vẫn chạy app cũ.

### Phía PC
`epcb_applib.ota` (xem spec epcb-applib cùng ngày): đọc .bin, tính checksum như firmware,
gửi theo luồng trên, báo tiến độ qua callback.

### Kiểm tra
- Test host: server nanoMODBUS + handler OTA với external flash giả (mảng RAM) — đủ luồng,
  gửi lại khối, sai offset, sai checksum, ABORT.
- Trên board: nạp app có `TASK_MBSLAVE_EN`, dùng `epcb_applib.ota` gửi bản build khác
  version, xác nhận boot lên version mới (cần anh chạy — nghe/nhìn log console).

## Ngoài phạm vi
- Modbus không chặn (state machine) cho master.
- Cập nhật bootloader qua Modbus.
- Áp vào các dự án đã sinh từ base (IPMS, OPMS, EACC…) — làm riêng sau khi base ổn.
