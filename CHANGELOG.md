# Lịch sử thay đổi

Dự án mới luôn clone theo **tag mới nhất** và ghi version base vào README của mình — đó là cách duy
nhất biết sau này dự án đang thiếu bản sửa nào. Bootloader có số version riêng (`BOOT_VER`, in ra
console lúc khởi động).

## Chưa phát hành

- **Repo:** thôi đưa file `.elf` trong `release/` vào git (~1,1 MB mỗi bản, repo phình theo từng
  bản). Git chỉ giữ `.bin`; `.bin` + `.elf` của mọi bản (v1.0.0 → v1.1.2) đính kèm ở
  [GitHub Releases](https://github.com/hohoanganh/ak-base-kit-pio/releases).
- **Tài liệu:** hai guide EPCB trong repo MCP (`epcb-platformio-build`, `epcb-start-project`) cập
  nhật theo v1.1.2.

## v1.3.0 — 25/09/2026

Bootloader **không đổi mã** (vẫn 0.0.3), chỉ nâng `-DAPP_VERSION` cho khớp base.

- **Nối OTA vào firmware slave:** `app_modbus.cpp` (nhánh `TASK_MBSLAVE_EN`) gọi
  `mb_ota_init(&ota_ops)` trước `nmbs_server_create` — `ota_ops` trỏ thẳng `fw_ext_erase`,
  `fw_ext_write`, `fw_ext_checksum` (external flash, `task_fw.cpp`) và `ota_commit` (dựng
  `firmware_header_t` rồi gọi `fw_commit_app_later`, hẹn `FW_MB_OTA_COMMIT` sau 200 ms để phản
  hồi Modbus kịp ra đường truyền trước khi ghi BSF + reset). `static_assert(MB_OTA_PSK ==
  FIRMWARE_PSK)` chặn lệch magic number ngay lúc biên dịch.
- **OTA qua Modbus RS485** dùng được thật trên env `app_mbslave`: state machine bảng thanh ghi
  holding `0xF000` (BEGIN/COMMIT/ABORT, ghi khối, đọc STATUS) — chi tiết bảng thanh ghi + luật
  (bin_len bội 4, khoá state sau COMMIT, chặn broadcast unit_id 0, ops NULL thì BEGIN/COMMIT/ghi
  khối lỗi an toàn) ở [docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md](docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md).
  README có mục "Cập nhật firmware qua RS485" hướng dẫn dùng từ PC.
- **Review nhỏ:** thêm chú thích "chi dung cho RTU: unit_id 0 la broadcast" cạnh hai chỗ chặn
  `unit_id == 0` trong `mb_slave_regs.c` (không đổi hành vi).
- **Số flash:**

  | Env | v1.2.0 | v1.3.0 |
  |---|---|---|
  | `app` | 53668 B | 53772 B |
  | `app_mbslave` | 56960 B | 58120 B |
  | `boot` | 6820 B | 6820 B (không đổi) |

- **Chưa kiểm trên phần cứng thật:** OTA qua RS485 mới chạy qua test host (giả lập
  read/write/checksum bằng gcc thường), chưa nạp board thật để kéo file `.bin` qua RS485.

## v1.2.0 — 25/09/2026

Bootloader **không đổi mã** (vẫn 0.0.3), chỉ nâng `-DAPP_VERSION` cho khớp base (base đánh số chung
app + boot).

- **Modbus master:** bỏ hẳn thư viện thương mại `mbmaster-v2.9.6`, chuyển sang
  [**nanoMODBUS v1.23.0**](https://github.com/debevv/nanoMODBUS) (MIT) — port RTU trên USART2 tự viết
  (`rs485_port.c`, ring buffer + DIR RS485), **không còn dùng TIM4** (trước phải chiếm timer cho
  timeout; xem commit `feat(modbus): master chuyen sang nanoMODBUS`).
- **Thêm chế độ Modbus SLAVE:** cờ `-DTASK_MBSLAVE_EN` (loại trừ lẫn nhau với `TASK_MBMASTER_EN`,
  `#error` lúc biên dịch nếu bật cả hai) — nanoMODBUS chạy vai server trên USART2, dùng cho OTA firmware
  qua RS485. Thanh ghi demo trong `mb_slave_regs.c`: reg 0 = `(major<<8)|minor`, reg 1 = patch, reg 2 =
  uptime (giây). Vòng lặp `app_modbus_poll()` gọi từ `task_polling_mbslave`
  (`AC_TASK_POLLING_MBSLAVE_ID`).
- **Env mới `[env:app_mbslave]`** trong `platformio.ini` — kế thừa `env:app`, chỉ đảo cờ
  master → slave (`build_unflags`/`build_flags`), build ra file riêng
  `release/app_mbslave/ak_base_kit_app_mbslave_v1.2.0.bin` (không đè lên bản `env:app`).
- **Test host:** `tests_host/modbus/run_tests.sh` biên dịch `mb_slave_regs.c` + `nanomodbus.c` bằng
  gcc thường (không cần board), kiểm giá trị 3 thanh ghi demo — chạy được trên máy dev/CI.
- **Timeout master đổi:** nanoMODBUS master chờ phản hồi tối đa `APP_MB_READ_TIMEOUT_MS` = 500 ms
  (`app_modbus.h`), thay vì 100 ms của `mbmaster` cũ. Hệ quả: lệnh `modbus r` khi rút mất một thiết
  bị giờ đợi ~2 s cho thiết bị đó (thay vì ~0,4 s trước đây) trước khi báo lỗi và sang thiết bị kế.
- **Số flash (env:app, không tính env:app_mbslave):**

  | Env | Trước (v1.1.2, còn mbmaster-v2.9.6) | Sau (v1.2.0, nanoMODBUS master) |
  |---|---|---|
  | `app` | 58220 B | 53668 B |
  | `app_mbslave` (mới) | — | 56960 B |
  | `boot` | 6820 B | 6820 B (không đổi) |

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
