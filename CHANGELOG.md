# Lịch sử thay đổi

Dự án mới luôn clone theo **tag mới nhất** và ghi version base vào README của mình — đó là cách duy
nhất biết sau này dự án đang thiếu bản sửa nào. Bootloader có số version riêng (`BOOT_VER`, in ra
console lúc khởi động).

## v1.1.2 — 24/09/2026

Bootloader **không đổi** (vẫn 0.0.3).

- **Sửa lỗi #12:** cờ biên dịch trong `pio_build_flags.py` (`-std=gnu99`, `-std=gnu++11`,
  `-fno-use-cxa-atexit`) chưa từng tới file nguồn nào — script chỉ sửa `env`, không sửa `projenv`.
  Nay áp cho cả hai; build đúng chuẩn gnu99/gnu++11 vẫn 0 cảnh báo.
- **Sửa lỗi #13:** version app gõ cứng `0.0.0.3`. Nay `-DAPP_VERSION` là nguồn duy nhất — board in
  `App version: 1.1.2.0`, khớp tên file release.
- **Kernel:** `task_create()` kiểm thứ tự `app_task_table` khớp enum task ID, lệch thì
  `FATAL("TK", 0x08)` ngay lúc khởi động. Kernel tra task bằng chỉ số `task_table[id]`, nên thêm task
  sai chỗ trước đây làm message đi nhầm task mà không báo gì.
- **Build:** giới hạn flash riêng từng env — `board_upload.maximum_size` app 116K, boot 8K. Trước đây
  cả hai tính trên 128K của chip: phần trăm báo sai, và bootloader lớn quá 8K không bị PlatformIO
  chặn.
- **Tài liệu:**
  - Vẽ lại sơ đồ bootloader theo code thật: sau cập nhật là `UPDATE_RES` (không phải `NONE`), có nhánh
    tự vá BSF và nhánh chờ nạp UART khi flash app trống.
  - Hướng dẫn thêm task: nói rõ ràng buộc thứ tự bảng task; sửa đoạn `platformio.ini` mẫu; sửa chỗ
    ghi flash trắng đọc ra `0xFF` (trên STM32L1 là `0x00`).
  - README: bước 1 đổi sang clone theo tag (khớp hướng dẫn), bổ sung cây thư mục, thêm mục phiên bản.
  - Thêm file này. Bỏ bản HTML trùng lặp (lỗi thời, cùng nội dung với bản Markdown).
- **Chuyển sang repo cá nhân:** bỏ thông tin liên hệ công ty và link website trong tài liệu, chú thích
  code; đường dẫn `_reference/...` (chỉ có trên máy cũ) đổi thành link repo gốc AK Foundation.

## v1.1.1 — 23/09/2026 · bootloader 0.0.3

Họ lỗi "struct cấu hình SPL không khởi tạo" — rà toàn bộ 62 biến `*_InitTypeDef`, 4 chỗ là lỗi thật:
ADC đọc kênh ngoài ra 0 (#8), rác vào điện trở kéo GPIOA, đo được chân RX RS485 ở trạng thái cấm
dùng (#9), SPI của bootloader (#10), NVIC buzzer (#11). Chi tiết: [docs/known-bugs.md](docs/known-bugs.md).

## v1.1.0 — 23/09/2026 · bootloader 0.0.2

Sửa 7 lỗi nền đầu tiên (#1–#7): `HardwareSerial` `flush()` rỗng và `write()` mất tín hiệu đánh thức
TXE; bootloader kẹt "uart boot" khi BSF hỏng — nay tự kiểm bảng vector app và tự vá BSF; USART2 một chủ
với cờ mới `SERIAL2_EN` / `SERIAL2_BAUDRATE`; tắt được `TASK_MBMASTER_EN`; macro chân USART2 đúng
silicon.

## v1.0.0 — 16/08/2026 · bootloader 0.0.1

Bản đầu tiên đánh tag: port AK Embedded Base Kit sang PlatformIO, target `-t bsf`, cờ linker
`max-page-size=4` chống ghi đè bootloader khi nạp bằng `-t upload`, tích hợp MCP docs server.
