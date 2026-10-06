# Bắt đầu dự án mới từ base `sources/`

Bản rút gọn để làm theo. Giải thích đầy đủ, code mẫu task, nguyên tắc kernel AK và cách port MCU khác: [huong-dan-su-dung-source-base.md](huong-dan-su-dung-source-base.md).

## 1. Bảy bước

1. **Clone theo tag mới nhất** (`git clone --depth 1 --branch v1.3.0 ...`), xoá `.git`, `git init`,
   ghi "Khởi tạo từ ak-base-kit-pio v1.3.0" vào README dự án, commit mốc "clean base". **Không** chép
   thư mục tay hay chép nền từ một dự án khác — mất dấu bản base là mất dấu các bản sửa.
2. **Đổi định danh** trong `platformio.ini`: `-DAPP_TITLE`, `-DAPP_VERSION` (cả `[env:app]` lẫn
   `[env:boot]`), đổi tên `build_dir`; đổi prefix tên file trong `pio_copy_release.py`.
3. **Chọn module** bằng define trong `[env:app]`: `TASK_MBMASTER_EN` (Modbus master, mặc định),
   `TASK_MBSLAVE_EN` (Modbus slave — dùng env `[env:app_mbslave]` có sẵn, hoặc tự đảo cờ), `SERIAL2_EN`,
   `IF_LINK_UART_EN`, `SSD1309_DRIVER_EN`/`SH1106_DRIVER_EN`, `TASK_ZIGBEE_EN` (tắt),
   `IF_NETWORK_NRF24_EN` (tắt)... kèm `build_src_filter` tương ứng. USART2 chỉ có **một chủ**:
   `TASK_MBMASTER_EN`, `TASK_MBSLAVE_EN` hoặc `SERIAL2_EN` — bật từ hai cờ trở lên là lỗi biên dịch.
4. **Sửa phần cứng** theo schematic board mới: `sources/application/platform/stm32l/io_cfg.h/.c` (chân
   GPIO — chỗ sửa nhiều nhất), `sys_cfg.c` (clock, console). Struct cấu hình SPL luôn qua
   `XXX_StructInit()` trước khi gán.
5. **Build thử cả 2 env** — phải 0 lỗi, 0 cảnh báo trước khi viết code mới; nạp boot + app, xác nhận
   console lên log, LED life nháy.
6. **Viết chức năng mới** theo mô hình AK: thêm task ID vào `task_list.h` → thêm dòng vào
   `app_task_table` trong `task_list.cpp` **đúng vị trí tương ứng** → tạo `app/task_xxx.cpp` (tự vào
   build) → post message khởi động trong `main_app()`. Kernel tra task bằng chỉ số
   `task_table[task_id]`, nên thứ tự dòng phải khớp thứ tự enum — lệch là `FATAL("TK", 0x08)` ngay
   lúc khởi động.
7. **Release**: tăng `-DAPP_VERSION`, build, lấy file trong `release/` bàn giao.

Chi tiết từng bước + code mẫu task + nguyên tắc kernel AK + port MCU khác:
[docs/huong-dan-su-dung-source-base.md](huong-dan-su-dung-source-base.md).

## 2. Build, nạp và phát hành

```bash
pio run -e app                 # build firmware ứng dụng (Modbus master, mặc định)
pio run -e app_mbslave         # build biến thể Modbus SLAVE (OTA qua RS485)
pio run -e boot                # build bootloader
pio run -e boot -t upload      # 1. nạp boot (board trắng phải nạp cả 2)
pio run -e app  -t upload      # 2. nạp app (ST-Link)
pio run -e app  -t bsf         # 3. nạp BSF mẫu — chỉ cần với bootloader cũ (< 0.0.2)
pio device monitor             # console UART1 115200
```

Thành phẩm tự copy về `release/app/` và `release/boot/`, tên kèm version từ `-DAPP_VERSION`. Git chỉ
giữ file **`.bin`**; file `.elf` (~1,1 MB mỗi bản, cần khi debug firmware đã phát hành) không đưa vào git
để repo khỏi phình theo từng bản. **`.bin` và `.elf` của mọi bản** (app + bootloader) tải ở trang
[Releases](https://github.com/hohoanganh/ak-base-kit-pio/releases). Phát hành bản mới thì đính kèm 4 file
đó vào Release của tag:

```bash
gh release create vX.Y.Z --verify-tag --notes-file notes.md release/app/*vX.Y.Z* release/boot/*vX.Y.Z*
```
Mỗi env có giới hạn flash riêng (`board_upload.maximum_size`: app 116K, boot 8K) — vượt là build báo
lỗi ngay, không để bootloader lấn sang BSF.

**Bước 3 không còn bắt buộc từ bootloader 0.0.2 (base v1.1.0).** Bootloader tự kiểm bảng vector của
app; BSF (`0x08002000`) chưa ai ghi hoặc bị xoá giữa chừng thì nó tự vá rồi chạy app. Board còn mang
bootloader cũ thì vẫn phải chạy `-t bsf` — thiếu bước này boot rơi vào nhánh "unexpected status" và
đứng ở `while(1)` nhấp nháy LED, nhìn từ ngoài giống hệt board treo. Chi tiết:
[docs/known-bugs.md](known-bugs.md) #3.

## 3. Ghi chú quan trọng

- **Lỗi đã biết — đã sửa hết trong v1.1.2:** [docs/known-bugs.md](known-bugs.md). Dự án tạo từ
  bản cũ hơn còn mang các lỗi nghiêm trọng: `HardwareSerial::write()` kẹt ring TX (giết RS485 bán
  song công), bootloader kẹt ở "uart boot" khi BSF bị xoá giữa chừng, và `io_cfg_adc1()` nạp rác vào
  ADC làm kênh ngoài đọc ra 0 — xem file đó để chép bản sửa sang.
- **`build_dir` nằm ở `%TEMP%`** (xem `platformio.ini`): project trong OneDrive, build tại chỗ dễ bị
  khóa file `.o` gây lỗi "Permission denied" ngẫu nhiên.
- **`-Wl,-z,max-page-size=4 -Wl,--nmagic` trong `pio_build_flags.py` là bắt buộc.** `-t upload` nạp
  bằng openocd `program firmware.elf`, mà openocd đọc *program header* chứ không đọc section. Mặc định
  `ld` căn segment theo trang 64K nên segment của app (đặt tại `0x08003000`) bị kéo `p_paddr` về
  `0x08000000` và nuốt thêm 12K rác ở đầu — nạp app sẽ ghi đè header ELF lên bootloader và xoá BSF,
  board chết ngay. Hai cờ này ép segment bắt đầu đúng `0x08003000`.
- **`pio run` ghi đè file trong `release/`** cùng tên version. Build thử thì tăng version hoặc
  `git checkout -- release` sau khi build, đừng commit nhầm bản build thử.
- `task_zigbee.cpp` bị loại khỏi build (như bản gốc); muốn bật thêm `-DTASK_ZIGBEE_EN` và bỏ dòng loại
  trừ trong `build_src_filter`. Zigbee tự bật `SERIAL2_EN`, nên phải tắt `TASK_MBMASTER_EN`.
- Thư mục `doc/` nặng (~95MB PDF) của bản gốc **không copy theo** — xem ở
  [repo gốc](https://github.com/the-ak-foundation/ak-base-kit-stm32l151). Thư viện nanoMODBUS chạy cả
  vai master (`env:app`, mặc định) lẫn slave (`env:app_mbslave`) — loại trừ lẫn nhau qua
  `TASK_MBMASTER_EN` / `TASK_MBSLAVE_EN`.
- Test host cho lớp Modbus (không cần board): `bash tests_host/modbus/run_tests.sh`.
- Các file `Makefile.mk` còn trong `sources/` chỉ để tham khảo, PlatformIO không dùng.
