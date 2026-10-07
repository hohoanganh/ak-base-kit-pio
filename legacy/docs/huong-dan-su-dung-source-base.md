# Hướng dẫn dùng source base `ak-base-kit-pio` cho dự án mới

STM32L151CBT6 · PlatformIO

> Đọc kèm: [ak-base-kit-pio-luong-hoat-dong.md](ak-base-kit-pio-luong-hoat-dong.md) (kiến trúc & luồng
> hoạt động) · [README.md](../README.md) (quy ước build) · [known-bugs.md](known-bugs.md) (lỗi đã biết)

---

## 1. Điểm hay của source base này

| # | Điểm mạnh | Ý nghĩa thực tế |
|---|-----------|-----------------|
| 1 | **Kernel AK — Active Object, không RTOS** | Task = hàm nhận message, chạy run-to-completion. Không context switch, không race condition kiểu preemptive, không cần tính stack riêng từng task. RAM footprint rất nhỏ (~vài KB) — vừa vặn MCU 16K RAM. Debug dễ: mọi hoạt động đều là message có thể log lại. |
| 2 | **Bootloader tách riêng + cập nhật firmware sẵn** | Boot 8K độc lập, hỗ trợ nạp firmware qua UART (không cần ST-Link khi đã ra sản phẩm) và tự cập nhật app từ external flash (nền tảng cho OTA). Sản phẩm thương mại dùng được ngay, không phải tự viết. |
| 3 | **Build bằng PlatformIO** | Cài 1 extension VS Code là build được — toolchain ARM tải tự động, không cần cài GCC/Make thủ công như bản gốc. IntelliSense, debug ST-Link, monitor console tích hợp sẵn. |
| 4 | **SPL + CMSIS vendor sẵn trong source** | Không phụ thuộc mạng/registry khi build (chỉ cần platform ststm32 lần đầu). Build lặp lại được sau nhiều năm — quan trọng với firmware sản phẩm có vòng đời dài. |
| 5 | **Module bật/tắt bằng cờ biên dịch** | Zigbee, nRF24, Modbus master, OLED (SSD1309/SH1106), UART link... bật/tắt bằng 1 dòng define trong `platformio.ini` — không sửa code. Dự án mới chỉ giữ module cần dùng. |
| 6 | **Kiến trúc phân lớp rõ ràng** | `app / kernel / driver / common / platform` tách biệt. Đổi phần cứng chỉ sửa `platform + driver`; logic nghiệp vụ ở `app/` không đụng. Nhiều người làm chung ít giẫm chân nhau. |
| 7 | **Hạ tầng debug sẵn có** | Shell console qua UART (115200) với `cmd_line`, log `xprintf`, watchdog kép (IWDG 32s + soft watchdog 20s), log queue message của kernel, `.non_clear_ram` giữ dữ liệu qua soft-reboot để phân tích lỗi. |
| 8 | **Release tự động có version** | Build xong tự copy `release/<env>/ak_base_kit_<env>_v<x.y.z>.bin` — không nhầm bản khi bàn giao sản xuất. Build dir đặt ngoài OneDrive nên không dính lỗi khóa file. |
| 9 | **Đã kiểm chứng** | Build sạch 0 cảnh báo với `-Wall` (app ~58 KB / boot ~6,8 KB), bootloader và driver nền đã chạy trên AK MCU KIT 3I0 — xem [known-bugs.md](known-bugs.md). Mô hình PlatformIO lấy từ một dự án sản phẩm đã chạy ổn định thực tế. |

**Khi nào KHÔNG nên dùng:** cần preemptive real-time cứng (deadline µs) → cân nhắc RTOS; MCU khác dòng STM32L1 → phải port lại `platform/` (xem mục 6).

---

## 2. Chuẩn bị

1. **VS Code** + extension **PlatformIO IDE** (tự kéo toolchain ARM khi build lần đầu — cần mạng lần đầu).
2. **ST-Link** (nạp + debug). Sản phẩm đã có boot thì nạp app qua UART cũng được.
3. Lấy source base **theo tag**, đừng copy thư mục tay:

```bash
git clone --depth 1 --branch v1.3.0 https://github.com/hohoanganh/ak-base-kit-pio.git my-project-fw
cd my-project-fw
rm -rf .git && git init
```

Rồi ghi ngay vào README của dự án mới:

> Khởi tạo từ ak-base-kit-pio **v1.3.0**

Một dòng thôi nhưng là thứ **duy nhất** giúp sau này biết dự án nào đang thiếu
fix nào của source base. Copy thư mục tay thì mất hẳn thông tin này, vài tháng
sau không ai nhớ bản gốc là bản nào.

> **Đừng làm việc trong thư mục OneDrive.** OneDrive khóa file trong `.git` và
> khóa file `.o` giữa lúc build (`ar.exe: unable to rename ...: Permission
> denied`). Để repo ở `C:\Work\` hoặc `D:\dev\`. Nếu buộc phải nằm trong
> OneDrive thì giữ dòng `build_dir = ${sysenv.TEMP}/...` trong `platformio.ini`
> (sửa tên cho khỏi đụng dự án khác).

## 3. Việc đầu tiên khi tạo dự án mới

### 3.1. Đặt tên & version — `platformio.ini`

```ini
[env:app]
build_flags =
    ${env.build_flags}
    -Os
    -DAPP_TITLE=\"my-project-app\"
    -DAPP_VERSION=\"0.1.0\"
    ; ... giữ nguyên các dòng còn lại
```

Làm tương tự với `[env:boot]`. Sửa luôn prefix tên file release trong `pio_copy_release.py` (chuỗi
`ak_base_kit_`).

### 3.2. Chọn module — `platformio.ini` `[env:app]`

| Cờ | Module | Mặc định |
|----|--------|----------|
| `-DTASK_MBMASTER_EN` | nanoMODBUS master RTU — giữ USART2, poll thiết bị tớ (cảm biến nhiệt độ/độ ẩm SHT35, relay LH-IO-01) | Bật |
| `-DTASK_MBSLAVE_EN` | nanoMODBUS **slave** RTU — giữ USART2, board đóng vai tớ (dùng cho OTA qua RS485). Loại trừ với `TASK_MBMASTER_EN`. Có sẵn env `[env:app_mbslave]` trong `platformio.ini` | Tắt |
| `-DSERIAL2_EN` | Arduino `Serial2` giữ USART2 (cầu UART…). Baud: `-DSERIAL2_BAUDRATE=9600`, mặc định 115200 | Tắt |
| `-DIF_LINK_UART_EN` | Giao thức link UART (3 task link) | Bật |
| `-DSSD1309_DRIVER_EN` / `-DSH1106_DRIVER_EN` | OLED 1.54"/1.3" | SSD1309 |
| `-DTASK_ZIGBEE_EN` | Zigbee | Tắt |
| `-DIF_NETWORK_NRF24_EN` | Mạng nRF24 | Tắt |
| `-DUSE_EXTERNAL_FLASH` | External SPI flash | Bật |

**USART2 chỉ có một chủ:** `TASK_MBMASTER_EN`, `TASK_MBSLAVE_EN` hoặc `SERIAL2_EN` — bật từ hai cờ trở
lên là `#error` lúc biên dịch (`app.h`). `TASK_ZIGBEE_EN` tự bật `SERIAL2_EN`. Tắt `TASK_MBMASTER_EN`
trả lại khoảng 4,5 KB flash (env `app`: 58220 → 53668 B tại v1.2.0, xem `CHANGELOG.md`).

Tắt module = xóa dòng define; nhớ bỏ/thêm dòng tương ứng trong `build_src_filter` nếu module có nhóm file riêng (xem comment trong `platformio.ini`).

### 3.2.1. Modbus slave — build sẵn `env:app_mbslave`

`platformio.ini` có sẵn env `[env:app_mbslave]` kế thừa `[env:app]` (`extends = env:app`), chỉ đảo cờ
master → slave:

```bash
pio run -e app_mbslave
```

Board chạy vai **server** Modbus RTU trên USART2 (`app_modbus.cpp`: tạo server nanoMODBUS +
`app_modbus_poll()` gọi từ task `task_polling_mbslave` / `AC_TASK_POLLING_MBSLAVE_ID`). Bảng thanh ghi
demo (`sources/application/networks/mb_port/mb_slave_regs.c`):

| Địa chỉ | Nội dung |
|---|---|
| 0 | `(major << 8) \| minor` — ví dụ v1.2.0 → `0x0102` |
| 1 | `patch` — ví dụ v1.2.0 → `0` |
| 2 | Uptime tính bằng giây kể từ lúc board khởi động |

Test lớp Modbus **không cần board** (biên dịch host bằng gcc, gọi thẳng `mb_slave_regs.c` +
`nanomodbus.c`):

```bash
bash tests_host/modbus/run_tests.sh
```

### 3.3. Cấu hình phần cứng — `sources/application/platform/stm32l/`

| File | Sửa gì |
|------|--------|
| `io_cfg.h` / `io_cfg.c` | **Chỗ sửa nhiều nhất** — định nghĩa chân GPIO: led, button, buzzer, SPI CS, ADC... theo schematic board mới |
| `sys_cfg.c` | Clock hệ thống (mặc định 32MHz HSI+PLL), console UART |
| `stm32l1xx_conf.h` | Bật/tắt driver SPL nếu cần thêm ngoại vi |

Bootloader có bộ file `platform/` riêng trong `sources/boot/` — thường không cần đụng.

## 4. Viết chức năng mới (task mới)

Quy trình thêm 1 task theo đúng mô hình AK (ví dụ `task_sensor`):

**Bước 1 — Khai báo ID** — `sources/application/app/task_list.h`, thêm ngay trước `AK_TASK_EOT_ID`:

```c
enum {
    ...
    AC_TASK_DISPLAY_ID,
    AC_TASK_SENSOR_ID,      /* task mới */
    AK_TASK_EOT_ID,
};
extern void task_sensor(ak_msg_t*);
```

**Bước 2 — Đăng ký vào bảng task** — `app_task_table` trong `task_list.cpp`, **ngay trước dòng
`AK_TASK_EOT_ID`**:

```c
{AC_TASK_SENSOR_ID,  TASK_PRI_LEVEL_4,  task_sensor},
```

> **Thứ tự dòng phải khớp thứ tự enum ở bước 1.** Kernel tra task bằng chỉ số
> `task_table[task_id]`, không tìm theo ID — dòng lệch chỗ là message đi nhầm task, không báo gì.
> Các khối `#if defined(...)` trong enum và trong bảng cũng phải bọc giống hệt nhau. Từ v1.1.2
> `task_create()` kiểm điều này lúc khởi động và dừng bằng `FATAL("TK", 0x08)` nếu lệch.

**Bước 3 — Tạo handler** — `app/task_sensor.cpp` + `.h`, định nghĩa signal trong `app.h` (hoặc header riêng):

```c
/* app.h */
enum {
    SENSOR_INIT = AK_USER_DEFINE_SIG,
    SENSOR_READ_TIMER,
};

/* task_sensor.cpp */
void task_sensor(ak_msg_t* msg) {
    switch (msg->sig) {
    case SENSOR_INIT:
        /* khởi tạo, đặt timer đọc chu kỳ 1s */
        timer_set(AC_TASK_SENSOR_ID, SENSOR_READ_TIMER, 1000, TIMER_PERIODIC);
        break;
    case SENSOR_READ_TIMER: {
        /* đọc sensor, gửi kết quả cho task khác (DISPLAY_SENSOR_UPDATE: signal
         * tự định nghĩa cho task_display) */
        uint16_t value = sensor_read();
        task_post_common_msg(AC_TASK_DISPLAY_ID, DISPLAY_SENSOR_UPDATE,
                             (uint8_t*)&value, sizeof(value));
        break;
    }
    default:
        break;
    }
}
```

**Bước 4 — Thêm file vào build** — `platformio.ini` đã có `+<application/app/*.cpp>` nên file mới trong `app/` **tự được build**, không phải sửa gì.

**Bước 5 — Kích hoạt lúc khởi động** — trong `main_app()` (`app.cpp`), post message đầu tiên:

```c
task_post_pure_msg(AC_TASK_SENSOR_ID, SENSOR_INIT);
```

**Nguyên tắc vàng của kernel AK:**

- Handler phải **ngắn** — xử lý xong message rồi return, không `while(1)`, không delay dài (chặn toàn bộ hệ thống). Việc chờ → dùng `timer_set` ONE_SHOT.
- Giao tiếp giữa task **chỉ qua message** (`task_post_pure_msg` / `common` / `dynamic`), không gọi hàm chéo giữa task, hạn chế biến toàn cục chia sẻ.
- Từ ISR muốn báo task: dùng `task_post` trong cặp `task_entry_interrupt()` / `task_exit_interrupt()` (xem mẫu trong `platform/stm32l/`).
- Ưu tiên: PRI_2 cho nghiệp vụ thường, PRI_4-5 cho giao tiếp, PRI_6-7 dành cho hệ thống (life/timer) — đừng đặt task nghiệp vụ ưu tiên cao hơn timer.

## 5. Build — nạp — debug

```bash
pio run -e app                 # build firmware ứng dụng
pio run -e boot                # build bootloader
pio run -e boot -t upload      # nạp boot (lần đầu / board trắng)
pio run -e app  -t upload      # nạp app
pio run -e app  -t bsf         # seed BSF - chỉ cần với bootloader cũ (< 0.0.2)
pio device monitor             # console UART1 115200
```

- **Board trắng nạp `boot` → `app` là đủ** với bootloader 0.0.2 trở lên (base v1.1.0 trở đi):
  bootloader tự kiểm bảng vector của app, thấy BSF chưa ai ghi thì tự vá rồi chạy app
  (console in `[BOOT] share boot repaired`). Board còn mang bootloader cũ thì vẫn cần
  bước `bsf` — xem [known-bugs.md](known-bugs.md) #3.
- **Vì sao bootloader cũ cần `bsf`:** bootloader chỉ nhảy sang app khi đọc được trong vùng
  *boot share flash* (`0x08002000`) cả hai điều kiện `fw_app_cmd.cmd ==
  SYS_BOOT_CMD_NONE` và `current_fw_app_header.psk == FIRMWARE_PSK`. Vùng này
  bình thường do luồng update UART/OTA ghi; nạp thẳng bằng ST-Link **không hề
  đụng tới nó**, nên trên chip mới BSF vẫn trắng (trên STM32L1 flash đã xoá đọc ra `0x00`, không
  phải `0xFF`) và bootloader rơi vào
  nhánh "unexpected status" — `while(1)` nháy LED, **nhìn từ ngoài y hệt board
  hỏng**. Chạy `-t bsf` một lần là xong, chỉ cần làm lại sau khi xóa toàn chip.
- **Không được bỏ cờ linker `-Wl,-z,max-page-size=4`** trong
  `pio_build_flags.py`: mặc định `ld` căn lề segment theo trang 64K, khiến
  `p_paddr` của app (ở `0x08003000`) bị kéo lùi về `0x08000000`. `pio run -t
  upload` nạp bằng openocd `program firmware.elf`, mà openocd đọc **program
  header** chứ không đọc section → sẽ ghi đè lên bootloader và xóa luôn BSF.
  Nạp xong là board chết ngay dù build báo thành công.
- Thành phẩm: `release/app/` và `release/boot/` (tự sinh sau mỗi lần build).
- Console có shell: gõ lệnh qua UART (xem `shell.cpp` để thêm lệnh mới — bảng `lgn_cmd_table`).
- Log bật/tắt bằng các define `SYS_PRINT_EN`, `APP_DBG_EN`... trong `platformio.ini`.

## 6. Port sang MCU khác (nâng cao)

### 6.1. Phải thay bao nhiêu?

Đo thực tế mức độ dính chip của từng lớp (đếm file có nhắc `stm32`):

| Lớp | Số file | Dính chip? |
|---|---|---|
| `ak/` (kernel) | 13 | **Không hề** — dùng lại nguyên |
| `common/` | 18 | **Không hề** |
| `sys/` | 7 | Gần như không |
| `driver/` | 22 | Chỉ **2/22** gọi thẳng SPL (`buzzer`, `nRF24`) |
| `platform/` | 126 | **Toàn bộ** — SPL, CMSIS, startup, linker, `io_cfg` |

Nói cách khác: đổi chip **không phải viết lại từ đầu**, gần như chỉ thay lớp
`platform/`. Đây chính là lợi ích của kiến trúc phân lớp.

### 6.2. Ba tình huống

**a. Cùng STM32L1, khác dung lượng flash/RAM** — nhẹ nhất. Sửa
`boards/*.json` (`maximum_size`), `board_upload.maximum_size` của từng env trong
`platformio.ini`, 2 file `ak.ld` (độ dài FLASH/RAM), và `APP_START_ADDR` nếu đổi layout.

**b. Khác dòng nhưng vẫn có SPL** (F1, F4, L1 khác) — ST có Standard
Peripheral Library cho các họ này. Thay `platform/stm32l/Libraries` bằng SPL
tương ứng, viết lại `io_cfg.h/.c` và `system.c`, sửa `ak.ld`. Trong
`platformio.ini` đổi `board`, `board_build.ldscript`, các đường dẫn `-I` và
`build_src_filter`.

**c. STM32 đời mới** (G0, G4, L4, U5, H7, C0...) — **ST KHÔNG làm SPL cho các
họ này**, chỉ có STM32Cube HAL/LL. Nghĩa là lớp `platform/` phải viết lại theo
HAL/LL chứ không port được từ SPL. Đây là điểm hay bị đánh giá thấp khi lên kế
hoạch: nặng hơn hẳn tình huống (b), dù kernel AK vẫn giữ nguyên. Hai driver
`buzzer` và `nRF24` cũng phải sửa theo.

### 6.3. Đổi chip là phải tính lại bản đồ flash

Bố cục hiện tại (boot 8K → BSF 4K @ `0x08002000` → app 116K) tính cho **flash
128K** của L151CB. Chip khác dung lượng thì phải sửa:

1. `ak.ld` của **cả** `boot` lẫn `app`
2. **`BSF_ADDR` trong `pio_bsf.py`** — rất hay quên. Quên cái này thì
   bootloader đọc BSF ở sai địa chỉ, không thấy cờ hợp lệ, và board nháy LED
   nhìn y hệt bị treo dù app đã nạp đúng.

## 7. Checklist bắt đầu dự án mới

- [ ] Clone **theo tag** (`--branch v1.3.0`), `rm -rf .git`, `git init`
- [ ] **Ghi version base vào README** dự án mới ("Khởi tạo từ ak-base-kit-pio v1.3.0")
- [ ] Đổi `APP_TITLE` / `APP_VERSION` (app + boot)
- [ ] Đổi tên `build_dir` và prefix file release
- [ ] Chọn module (define) + `build_src_filter` tương ứng
- [ ] Sửa `io_cfg.h/.c` theo schematic board
- [ ] Build thử `pio run -e app` và `-e boot` — phải 0 lỗi, 0 cảnh báo trước khi viết code mới
- [ ] Nạp `boot` → nạp `app` (bootloader cũ < 0.0.2 thì thêm **`pio run -e app -t bsf`**, thiếu là board nháy LED như treo)
- [ ] Xác nhận console lên log và LED life nhấp nháy
- [ ] Xóa task mẫu không dùng (display/zigbee/rf24...) hoặc để lại tham khảo
- [ ] Commit mốc "clean base" trước khi phát triển

