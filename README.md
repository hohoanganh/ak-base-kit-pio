# ak-base-kit-pio — EPCB Firmware Source Base (STM32L151CBT6)

Source base phát triển firmware EPCB, dựng lại từ **ak-base-kit-stm32l151** (nguồn gốc, build bằng Makefile) theo mô hình PlatformIO đã chạy ổn định của **Smart-PDU-firmware**. Bare-metal, không dùng framework PlatformIO nào — SPL + CMSIS + startup + linker script đều nằm sẵn trong `sources/`.

## Cấu trúc

```
ak-base-kit-pio/
├── platformio.ini            # cấu hình build: env app + env boot
├── boards/genericSTM32L151CB_bare.json
├── pio_build_flags.py        # cờ riêng C/C++ + cờ link (nostartfiles, nano.specs...)
├── pio_copy_release.py       # tự copy .bin/.elf về release/<env>/ sau khi build
├── release/                  # firmware thành phẩm (tự sinh)
└── sources/
    ├── application/          # firmware ứng dụng (AK kernel, task, driver, libs)
    │   ├── ak/               # kernel AK (fsm, tsm, task, timer, message)
    │   ├── app/              # task ứng dụng + screens
    │   ├── common/           # utils, xprintf, cmd_line, container, view
    │   ├── driver/           # button, buzzer, eeprom, flash, gpio, led, OLED
    │   ├── libraries/        # ArduinoJson, nlohmann, QRCode
    │   ├── networks/         # net/link (UART link), mbmaster v2.9.6
    │   ├── platform/stm32l/  # io_cfg, sys_cfg, startup, SPL + CMSIS, ak.ld
    │   └── sys/
    └── boot/                 # bootloader 8K (cấu trúc tương tự, rút gọn)
```

## Memory map (giữ nguyên AK base kit)

| Vùng | Địa chỉ | Kích thước |
|------|---------|-----------|
| boot | 0x08000000 | 8K |
| BSF (boot share data) | 0x08002000 | 4K |
| app | 0x08003000 | 116K |

## Build & nạp

```bash
pio run -e app              # build firmware ứng dụng
pio run -e boot             # build bootloader
pio run -e app -t upload    # nạp qua ST-Link
pio device monitor          # console UART1 115200
```

Thành phẩm tự động copy về `release/app/` và `release/boot/`.

## Ghi chú quan trọng

- **build_dir nằm ở %TEMP%** (xem `platformio.ini`): project nằm trong OneDrive, build tại chỗ dễ bị OneDrive khóa file `.o` giữa chừng gây lỗi "Permission denied" ngẫu nhiên.
- Bật/tắt module (zigbee, nRF24, OLED SH1106/SSD1309, modbus master...) bằng các define trong `[env:app]` của `platformio.ini` — danh sách khớp `application/Makefile` gốc.
- `task_zigbee.cpp` bị loại khỏi build (TASK_ZIGBEE_EN tắt như bản gốc); muốn bật, thêm `-DTASK_ZIGBEE_EN` và bỏ dòng loại trừ trong `build_src_filter`.
- Thư mục `doc/` nặng (~95MB PDF datasheet) và các demo/tests/tools của mbmaster cho platform khác **không copy theo** — xem bản gốc tại `_reference/ak-base-kit-stm32l151-main`.
- Makefile gốc vẫn nằm rải rác trong `sources/` (các file `Makefile.mk`) — chỉ mang tính tham khảo, PlatformIO không dùng đến.

## Nguồn gốc

- Base: `_reference/ak-base-kit-stm32l151-main` (AK Embedded Base Kit, GaoKong)
- Mô hình PlatformIO: `_reference/Smart-PDU-firmware`
