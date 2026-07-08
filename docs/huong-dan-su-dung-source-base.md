# Hướng Dẫn Sử Dụng Source Base `ak-base-kit-pio` Cho Dự Án Mới

**EPCB® IOT SERVICES** · STM32L151CBT6 · PlatformIO

> Đọc kèm: `docs/ak-base-kit-pio-luong-hoat-dong.md` (kiến trúc & luồng hoạt động) · `README.md` (quy ước build)

---

## 1. Điểm Hay Của Source Base Này

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
| 9 | **Đã kiểm chứng** | Toàn bộ 143 file được compile-check chéo + build thật thành công (app 58KB / boot 6.8KB). Mô hình PlatformIO copy từ dự án Smart-PDU đã chạy ổn định thực tế. |

**Khi nào KHÔNG nên dùng:** cần preemptive real-time cứng (deadline µs) → cân nhắc RTOS; MCU khác dòng STM32L1 → phải port lại `platform/` (xem mục 6).

---

## 2. Chuẩn Bị

1. **VS Code** + extension **PlatformIO IDE** (tự kéo toolchain ARM khi build lần đầu — cần mạng lần đầu).
2. **ST-Link** (nạp + debug). Sản phẩm đã có boot thì nạp app qua UART cũng được.
3. Copy toàn bộ thư mục `ak-base-kit-pio/` → đổi tên theo dự án, ví dụ `my-project-fw/`.

> Nếu project nằm trong OneDrive: giữ nguyên dòng `build_dir = ${sysenv.TEMP}/...` trong `platformio.ini` (sửa tên cho khỏi đụng dự án khác), tránh lỗi "Permission denied" do OneDrive khóa file `.o`.

## 3. Việc Đầu Tiên Khi Tạo Dự Án Mới

### 3.1. Đặt tên & version — `platformio.ini`

```ini
[env:app]
    -DAPP_TITLE=\"my-project-app\"
    -DAPP_VERSION=\"0.1.0\"
```

Sửa luôn prefix tên file release trong `pio_copy_release.py` (chuỗi `ak_base_kit_`).

### 3.2. Chọn module — `platformio.ini` `[env:app]`

| Cờ | Module | Mặc định |
|----|--------|----------|
| `-DTASK_MBMASTER_EN` | Modbus master RTU | Bật |
| `-DIF_LINK_UART_EN` | Giao thức link UART (3 task link) | Bật |
| `-DSSD1309_DRIVER_EN` / `-DSH1106_DRIVER_EN` | OLED 1.54"/1.3" | SSD1309 |
| `-DTASK_ZIGBEE_EN` | Zigbee | Tắt |
| `-DIF_NETWORK_NRF24_EN` | Mạng nRF24 | Tắt |
| `-DUSE_EXTERNAL_FLASH` | External SPI flash | Bật |

Tắt module = xóa dòng define; nhớ bỏ/thêm dòng tương ứng trong `build_src_filter` nếu module có nhóm file riêng (xem comment trong `platformio.ini`).

### 3.3. Cấu hình phần cứng — `sources/application/platform/stm32l/`

| File | Sửa gì |
|------|--------|
| `io_cfg.h` / `io_cfg.c` | **Chỗ sửa nhiều nhất** — định nghĩa chân GPIO: led, button, buzzer, SPI CS, ADC... theo schematic board mới |
| `sys_cfg.c` | Clock hệ thống (mặc định 32MHz HSI+PLL), console UART |
| `stm32l1xx_conf.h` | Bật/tắt driver SPL nếu cần thêm ngoại vi |

Bootloader có bộ file `platform/` riêng trong `sources/boot/` — thường không cần đụng.

## 4. Viết Chức Năng Mới (Task Mới)

Quy trình thêm 1 task theo đúng mô hình AK (ví dụ `task_sensor`):

**Bước 1 — Khai báo ID** — `sources/application/app/task_list.h` (ID phải tăng dần, thêm trước `AK_TASK_EOT_ID`):

```c
enum {
    ...
    AC_TASK_DISPLAY_ID,
    AC_TASK_SENSOR_ID,      /* task mới */
    AK_TASK_EOT_ID,
};
extern void task_sensor(ak_msg_t*);
```

**Bước 2 — Đăng ký vào bảng task** — `task_list.cpp`:

```c
{AC_TASK_SENSOR_ID,  TASK_PRI_LEVEL_4,  task_sensor},
```

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
    case SENSOR_READ_TIMER:
        /* đọc sensor, gửi kết quả cho task khác */
        task_post_common_msg(AC_TASK_DISPLAY_ID, DISPLAY_SENSOR_UPDATE,
                             (uint8_t*)&value, sizeof(value));
        break;
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

## 5. Build — Nạp — Debug

```bash
pio run -e app                 # build firmware ứng dụng
pio run -e boot                # build bootloader
pio run -e boot -t upload      # nạp boot (lần đầu / board trắng)
pio run -e app  -t upload      # nạp app
pio device monitor             # console UART1 115200
```

- **Board trắng phải nạp cả boot lẫn app** (2 vùng flash khác nhau — xem memory map trong tài liệu luồng hoạt động).
- Thành phẩm: `release/app/` và `release/boot/` (tự sinh sau mỗi lần build).
- Console có shell: gõ lệnh qua UART (xem `shell.cpp` để thêm lệnh mới — bảng `lgn_cmd_table`).
- Log bật/tắt bằng các define `SYS_PRINT_EN`, `APP_DBG_EN`... trong `platformio.ini`.

## 6. Port Sang MCU Khác (Nâng Cao)

Cùng dòng STM32L1 dung lượng khác: sửa `boards/*.json` (`maximum_size`), 2 file `ak.ld` (độ dài FLASH/RAM) và `APP_START_ADDR` nếu đổi layout. Khác dòng (F1/F4/G0...): thay `platform/stm32l/` bằng SPL/HAL + startup tương ứng, giữ nguyên `ak/`, `app/`, `common/`, `driver/` (driver chỉ đụng `io_cfg`) — đây chính là lợi ích của kiến trúc phân lớp.

## 7. Checklist Bắt Đầu Dự Án Mới

- [ ] Copy folder, đổi tên; đổi `APP_TITLE` / `APP_VERSION` (app + boot)
- [ ] Đổi tên `build_dir` và prefix file release
- [ ] Chọn module (define) + `build_src_filter` tương ứng
- [ ] Sửa `io_cfg.h/.c` theo schematic board
- [ ] Build thử `pio run -e app` và `-e boot` — phải 0 lỗi trước khi viết code mới
- [ ] Nạp boot + app, xác nhận console lên log và LED life nhấp nháy
- [ ] Xóa task mẫu không dùng (display/zigbee/rf24...) hoặc để lại tham khảo
- [ ] `git init` — commit mốc "clean base" trước khi phát triển

---

*EPCB Vietnam · contact@epcb.vn · (+84) 367 939 867 · www.epcb.vn*
