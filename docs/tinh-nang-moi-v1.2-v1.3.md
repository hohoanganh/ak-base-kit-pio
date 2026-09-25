# Tính năng mới AK Base Kit v1.2.0 – v1.3.0

Cập nhật: 25/09/2026 · Board: AK Base Kit STM32L151CBT6 · Bootloader: 0.0.3 (không đổi)

Hai bản này thay toàn bộ lớp Modbus của source base và thêm đường **cập nhật firmware qua RS485**
không cần ST-Link. Mọi thay đổi đều đã chạy thật trên board (mục 5).

| Bản | Nội dung chính |
|---|---|
| v1.2.0 | Bỏ thư viện Modbus thương mại `mbmaster-v2.9.6`, chuyển sang **nanoMODBUS** (MIT); thêm chế độ **Modbus slave** và env `app_mbslave` |
| v1.3.0 | **OTA qua Modbus RS485** trên env `app_mbslave`, dùng lại nguyên đường external flash → BSF → bootloader sẵn có |

---

## 1. Vì sao đổi thư viện Modbus

- `mbmaster-v2.9.6` là bản **thương mại** của embedded-solutions.at ("Copyright … All rights
  reserved", không kèm license). Mọi dự án sinh từ base đều mang theo nó → rủi ro bản quyền.
- Nó chỉ có vai master, không làm được slave — mà muốn nạp firmware qua RS485 thì board phải là slave.
- [nanoMODBUS v1.23.0](https://github.com/debevv/nanoMODBUS) (MIT, một cặp `.c/.h`, không cấp phát
  động) làm được cả client (master) lẫn server (slave). Chép nguyên bản vào
  `sources/application/networks/nanomodbus/`, **không sửa**.

Lợi thêm: port mới **không chiếm TIM4** nữa, và flash env `app` giảm từ 58220 B xuống 53668 B
(≈ 4,5 KB).

## 2. Lớp Modbus mới

```
sources/application/
├── networks/nanomodbus/      thư viện gốc (MIT)
├── networks/mb_port/
│   ├── rs485_port.c/.h       USART2 + chân DIR RS485: ngắt RX → ring buffer 256 B, gửi polling TXE/TC
│   ├── mb_slave_regs.c/.h    bảng thanh ghi của slave (demo + chuyển vùng 0xF000 sang OTA)
│   └── mb_ota.c/.h           máy trạng thái OTA (C thuần, test được trên PC)
└── app/app_modbus.cpp/.h     tạo client/server nanoMODBUS, poll slave, nối OTA vào task_fw
```

### Chọn chế độ bằng cờ biên dịch

| Env PlatformIO | Cờ | Vai | Dùng khi |
|---|---|---|---|
| `app` (mặc định) | `TASK_MBMASTER_EN` + `NMBS_SERVER_DISABLED` | master | Board đọc cảm biến/relay RS485 (lệnh console `modbus r`) |
| `app_mbslave` | `TASK_MBSLAVE_EN` + `NMBS_CLIENT_DISABLED` | slave, địa chỉ 1 | Board được máy tính/gateway đọc, và **nhận firmware qua RS485** |

- Hai cờ dùng chung USART2 nên **loại trừ nhau** — bật cả hai là `#error` ngay lúc biên dịch
  (tương tự với `SERIAL2_EN`/`TASK_ZIGBEE_EN`).
- Cờ `NMBS_*` phải là cờ toàn cục (đổi layout struct `nmbs_callbacks`) — đã đặt sẵn trong
  `platformio.ini`, đừng chuyển vào từng file.
- Thông số trong `app_modbus.h`: 9600 baud, 8N1; master chờ phản hồi 500 ms; khoảng lặng tối đa
  giữa hai byte 20 ms.

### Thanh ghi demo của slave (FC03)

| Thanh ghi | Nội dung |
|---|---|
| 0 | phiên bản app `(major << 8) \| minor` — v1.3.0 đọc ra `0x0103` |
| 1 | patch |
| 2 | uptime (giây, 16 bit) |

Dự án thật thêm thanh ghi của mình vào `mb_slave_regs.c` (callback đọc/ghi holding).

### Khác biệt so với mbmaster cũ (khi nâng dự án cũ lên base mới)

- Kiểu mbmaster (`USHORT`, `ULONG`, `UCHAR`, `eMBErrorCode`, `xMBHandle`) không còn — dùng
  `uint16_t`/`uint32_t`/`uint8_t` và `nmbs_error` (so với `NMBS_ERROR_NONE`).
- `appMBMasterRead()` giữ nguyên tên và tham số; `appMBMasterWrite()` (vốn là code chết trong
  `#if 0`) đã xóa.
- Master chờ phản hồi 500 ms thay vì 100 ms: rút mất một thiết bị thì `modbus r` đợi ~2 s cho thiết
  bị đó (trước ~0,4 s) rồi mới báo lỗi. Watchdog vẫn được nuôi trong lúc chờ.

## 3. OTA qua Modbus RS485 (env `app_mbslave`)

### Cách hoạt động

```
PC (epcb_applib.ota)                    Board (app_mbslave)                         Bootloader 0.0.3
────────────────────                    ───────────────────                         ────────────────
ABORT (dọn phiên cũ nếu có)
ghi header: độ dài, checksum, psk  ──►  kiểm psk, độ dài ≤ 116 KB, bội 4
BEGIN                              ──►  xóa 128 KB external flash (≈1 s)
khối 128 B × N (offset tuần tự)    ──►  ghi vào external flash 0x80000
COMMIT                             ──►  tính checksum trên external flash
                                        đúng → hẹn 200 ms → ghi BSF + reset  ──►  xóa vùng app, chép từ
                                                                                   external flash, kiểm
                                                                                   checksum, chạy app mới
đọc lại phiên bản + uptime         ◄──  app mới trả lời
```

Bootloader **không sửa một dòng nào** — OTA chỉ là một đường mới để đưa ảnh vào external flash, phần
sau đi đúng đường cập nhật firmware đã có từ trước.

### Bảng thanh ghi (holding, base `0xF000`)

| Thanh ghi | Đọc/ghi | Nội dung |
|---|---|---|
| `0xF000` | W | CMD: 1 = BEGIN, 2 = COMMIT, 3 = ABORT |
| `0xF001` | R | STATUS: 0 rảnh, 1 đang nhận, 2 đã commit (sắp reset), `0x8001` sai header, `0x8002` sai thứ tự khối, `0x8003` sai checksum, `0x8004` quá cỡ |
| `0xF002–0xF003` | R/W | độ dài ảnh (hi, lo) — **phải là bội số của 4** |
| `0xF004` | R/W | checksum = tổng các word 32-bit little-endian của ảnh, lấy 16 bit thấp |
| `0xF005–0xF006` | R/W | psk (hi, lo) = `0x1A2B3C4D` |
| `0xF007–0xF008` | R | số byte đã nhận (hi, lo) |
| `0xF010–0xF011` | W | offset khối (hi, lo) |
| `0xF012–0xF051` | W | dữ liệu khối, tối đa 64 thanh ghi = 128 byte, byte cao trước |

### Các luật an toàn

- **Khối phải liền mạch.** Gửi lại đúng khối vừa gửi (PC mất phản hồi) → trả OK, không ghi lại.
  Nhảy cóc hoặc vượt độ dài → exception 04, STATUS `0x8002`, phiên hỏng — phải bắt đầu lại từ BEGIN.
- **Độ dài phải chia hết cho 4** (PC đệm `0xFF`). Lý do: bootloader đệm `0x00` khi chép phần lẻ,
  checksum sẽ lệch và board kẹt vòng reset.
- **Khóa sau COMMIT:** trong 200 ms chờ reset chỉ nhận COMMIT lặp lại (trả OK); BEGIN, ABORT và ghi
  header đều bị từ chối — không gì xóa được external flash khi bootloader sắp chép.
- **Không nhận broadcast** (địa chỉ 0): lệnh OTA gửi broadcast bị bỏ qua, để một lệnh nhầm không xóa
  flash của mọi board trên cùng đường dây.
- **Đứt giữa chừng không làm chết board:** vùng app chỉ bị đụng tới sau khi checksum ảnh mới đã
  đúng.

| Mất điện / đứt dây lúc… | Kết quả |
|---|---|
| đang gửi khối | app cũ chạy tiếp; chạy lại tool là xong (tool tự ABORT phiên dở) |
| trong 200 ms sau COMMIT | app cũ chạy tiếp (BSF chưa ghi); tool báo không xác nhận được phiên bản |
| bootloader đang chép | lần khởi động sau bootloader chép lại từ external flash (ảnh vẫn nguyên) |

### Dùng từ máy tính

Cần `epcb-applib` ≥ 1.4.0 (`pip install -e "D:/OneDrive/05_Shared_Libraries/epcb-applib[serial]"`).

```bash
python -m epcb_applib.ota COM16 release/app_mbslave/ak_base_kit_app_mbslave_v1.3.0.bin --slave 1 --baud 9600
```

Công cụ tự:
- từ chối file không phải ảnh app (`.elf`, `.hex`, bin bootloader, bin rác) bằng cách kiểm bảng vector;
- đệm ảnh cho đủ bội 4 và tránh checksum bằng 0 (bootloader bỏ qua header có checksum 0);
- gửi ABORT trước để dọn phiên dở;
- sau COMMIT đợi board khởi động lại, **chỉ tin câu trả lời có uptime nhỏ hơn thời gian từ lúc
  COMMIT**, rồi so phiên bản board báo với số phiên bản trong tên file — lệch thì thoát mã 1.

Tùy chọn: `--abort` (chỉ hủy phiên đang treo), `--no-verify` (bỏ bước xác nhận phiên bản).

🔴 **Chỉ gửi ảnh `app_mbslave`.** Ảnh `env:app` (master) cũng qua được kiểm tra vector nhưng không
có slave — nạp xong board mất đường OTA, phải quay lại ST-Link.

## 4. Nạp bằng ST-Link

`pio run -e <env> -t upload` có thể lỗi `Debug adapter doesn't support 'hla_swd' transport` với
OpenOCD mới trong PlatformIO và ST-Link V2 (gặp với FW `V2J37M26`). Khi đó nạp bằng ST-LINK Utility:

```bash
ST-LINK_CLI.exe -c SWD -P release/boot/ak_base_kit_boot_v1.3.0.bin 0x08000000 -V
ST-LINK_CLI.exe -c SWD -P release/app_mbslave/ak_base_kit_app_mbslave_v1.3.0.bin 0x08003000 -V -Rst
```

Nên đọc lưu flash cũ trước khi nạp đè: `ST-LINK_CLI.exe -c SWD -Dump 0x08000000 0x20000 backup.bin`.

## 5. Kết quả kiểm trên board thật (25/09/2026)

Board AK Base Kit, console qua ST-Link VCP (115200), RS485 qua USB-RS485 FTDI, slave 1 @ 9600.

| Thử nghiệm | Kết quả |
|---|---|
| Nạp boot + `app_mbslave` v1.3.0 | Verification OK, console `App version: 1.3.0.0`, `Init modbus slave >> 0` |
| Đọc FC03 thanh ghi 0..2 liên tục | 200/200 đúng (`0103`, `0000`, uptime) |
| OTA 1.3.0 → 1.3.1 → 1.3.0 | ≈ 78 s mỗi lượt (58 KB); bootloader xóa 0,8 s + chép 2,7 s, "internal checksum correctly" |
| Tool tự xác nhận sau khi nạp | báo đúng "phiên bản 1.3.1", rồi "1.3.0", mã thoát 0 |
| Giết tool ở 19 KB / 58 KB | board vẫn chạy app cũ (STATUS kẹt 1); chạy lại tool → thành công |
| Gửi `.elf`, bin bootloader, `--slave 0`, sai địa chỉ | đều bị từ chối / báo lỗi rõ, board không bị đụng tới |
| `--abort` | hủy được phiên dở |

Test tự động: `bash tests_host/modbus/run_tests.sh` (gcc trên PC: bảng thanh ghi slave + toàn bộ
máy trạng thái OTA qua đường truyền giả) và `pytest tests/test_ota.py` trong epcb-applib (75 test).

**Chưa kiểm:** chế độ master (`modbus r`) với cảm biến ES35-SW thật; đo chân DIR RS485 bằng máy
hiện sóng; rút điện đúng lúc bootloader đang chép.

## 6. Tài liệu liên quan

- Thiết kế chi tiết: [docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md](superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md)
- Kế hoạch triển khai: [docs/superpowers/plans/2026-09-25-nanomodbus-ota-va-gop-modbus-python.md](superpowers/plans/2026-09-25-nanomodbus-ota-va-gop-modbus-python.md)
- Hướng dẫn dùng base: [docs/huong-dan-su-dung-source-base.md](huong-dan-su-dung-source-base.md)
- Lịch sử thay đổi: [CHANGELOG.md](../CHANGELOG.md)
