# Port ak-mcu-base sang chip mới

Code phía trên HAL (kernel, services, boot, app) không đổi. Một port gồm:

| File | Nội dung |
|---|---|
| `port/<chip>/port_cfg.h` | bản đồ flash, RAM, tên board, chân console/LED |
| `port/<chip>/startup.c` | vector table, `reset_handler`, nhảy app (bản boot), fault handler |
| `port/<chip>/port_<chip>.c` | cài đặt `ak_port.h`, `hal.h`, `hal_flash.h`, `_sbrk` |
| `port/<chip>/boot.ld`, `app.ld` | linker script; `app.ld` đặt `.fw_header` (256 B) ở đầu APP |
| `port/<chip>/fw_header.c` | header ảnh nằm sẵn trong `.elf` của app |
| `port/<chip>/port.cmake` | target `boot`/`app` + bước `mkimage.py patch` sau khi build |

Bắt đầu bằng cách chép `port/stm32l151/` rồi sửa. Thêm nhánh `elseif(AK_PORT STREQUAL "<chip>")`
vào `CMakeLists.txt`.

## Danh sách hàm phải có

**`ak_port.h`** — `ak_port_enter_critical/exit_critical` (cho phép gọi lồng nhau, an toàn trong
ISR), `ak_port_millis`, `ak_port_fatal` (không trả về), `ak_port_idle`.

**`hal.h`** — `hal_init`, `hal_millis`, `hal_delay_ms`, `hal_reset_reason`, `hal_reset`,
`hal_board_name`, `hal_jump_to_app`, `hal_vector_ok`, `hal_console_putc/getc/flush`,
`hal_led_set/toggle`, `hal_wdt_start/kick`, `hal_nvm_read/write`. SysTick (1 ms) gọi
`hal_tick_hook(1)` — app định nghĩa hàm này để chạy timer của kernel.

**`hal_flash.h`** — `hal_flash_info/erase/write/read` theo partition BOOT / APP / STAGING.

## Những điểm cần chú ý

- **Giá trị flash sau khi xóa:** STM32L1 là `0x00`, đa số chip khác là `0xFF`. Khai đúng
  `erased_val`; test host chạy cả hai giá trị.
- **Căn lề vector table:** trên Cortex-M3/M4, VTOR phải căn theo kích thước bảng (lũy thừa 2).
  Header 256 B chỉ đủ khi bảng ≤ 64 vector. Chip nhiều IRQ hơn thì tăng `ALIGN` trong `sections.ld`
  và địa chỉ app trong `fw_image.h` / mkimage cho khớp.
- **Kích thước trang:** `boot_install()` cần trang APP ≥ 256 B và là bội của 128 (buffer chép
  128 B, bằng một half-page của STM32L1). STAGING được phép khác kiểu với APP (ví dụ SPI NOR: sector
  4K, ghi từng byte, xóa ra `0xFF`) nhưng phải lớn hơn hoặc bằng APP. Chip có sector lớn (16–128K,
  ví dụ STM32F4) thì phải chia lại partition theo sector.
- **STAGING trên flash ngoài:** xem `port/stm32l151/spi_nor.c` và cách `hal_flash_*` điều phối theo
  partition. Nếu không phát hiện được chip lúc khởi động thì trả `size = 0` cho STAGING: OTA sẽ báo
  lỗi gọn gàng thay vì ghi bừa.
- **NVM:** chip không có EEPROM thì dùng một trang flash riêng cho `boot_ctrl`. Nên ghi kiểu
  append (nhiều bản ghi trong trang, bản ghi cuối hợp lệ là bản đang dùng) để đỡ hao mòn flash.
- **App không được ghi APP/BOOT:** port STM32L1 chặn ngay trong `hal_flash_erase/write` theo
  `AK_BOOTLOADER`. Port mới nên làm giống vậy.
- **`.noinit` chung giữa boot và app:** đặt ở đầu RAM trong cả hai linker script, dùng để
  truyền yêu cầu nhảy app, nguyên nhân reset và thông tin fault.

## Kiểm port mới

1. `make test` vẫn phải xanh (không phụ thuộc port).
2. Build port mới, xem `--print-memory-usage`, rồi `python3 tools/mkimage.py info build/<chip>/app.img`.
3. Trên board: log boot → `ak_fw.py info` → OTA → rút điện giữa lúc boot đang cài.
