# CLAUDE.md

Hướng dẫn cho AI assistant làm việc trong repo này. Đọc file này trước khi sửa code.

> Tài liệu người đọc: [`README.md`](README.md) · [`docs/huong-dan-su-dung-source-base.md`](docs/huong-dan-su-dung-source-base.md) (dùng base cho dự án mới) · [`docs/ak-base-kit-pio-luong-hoat-dong.md`](docs/ak-base-kit-pio-luong-hoat-dong.md) (kiến trúc & luồng chạy).

## 1. Repo này là gì

Firmware **bare-metal** cho **STM32L151CBT6** (Cortex-M3, 32 MHz, 128K Flash / 16K SRAM), build bằng **PlatformIO** với `framework =` (rỗng — **không dùng** framework Arduino/SPL của PlatformIO). Đây là **source base** để copy sang dự án mới, không phải sản phẩm cuối.

Hai firmware độc lập, build từ cùng một project:

| env | Nguồn | Địa chỉ | Kích thước | Linker script |
|-----|-------|---------|-----------|---------------|
| `boot` | `sources/boot/` | `0x08000000` | 8K | `sources/boot/platform/stm32l/ak.ld` |
| — | (BSF — boot share data) | `0x08002000` | 4K | — |
| `app` | `sources/application/` | `0x08003000` | 116K | `sources/application/platform/stm32l/ak.ld` |

`app` chạy trên **kernel AK** (Active Object / message-driven, **không RTOS**). `boot` là bootloader tự viết, hỗ trợ nạp firmware qua UART và copy firmware từ external SPI flash vào internal flash.

SPL StdPeriph + CMSIS + startup + linker script đã được **vendor sẵn** trong `sources/{application,boot}/platform/stm32l/` — build không phụ thuộc registry.

## 2. Lệnh thường dùng

```bash
pio run -e app                 # build firmware ứng dụng (mặc định)
pio run -e boot                # build bootloader
pio run -e app  -t upload      # nạp app  (ST-Link)
pio run -e boot -t upload      # nạp boot (board trắng phải nạp cả 2)
pio run -t clean               # clean
pio device monitor             # console UART1 @ 115200
```

Không có test suite, không có linter, không có CI. **Cách kiểm chứng thay đổi duy nhất là build sạch cả hai env** (`pio run -e app && pio run -e boot`) — luôn build cả hai khi đụng vào `sources/`, kể cả khi chỉ sửa một bên, vì `boot/` và `application/` có bản sao riêng của nhiều file (`sys_boot`, `flash`, `led`, `xprintf`, SPL...).

**Cảnh báo môi trường:** `platformio.ini` đặt `build_dir = ${sysenv.TEMP}/pio_build_ak_base_kit` (tránh OneDrive khóa file `.o` trên máy Windows của tác giả). Trên Linux/macOS biến `TEMP` thường không tồn tại → path hỏng; khi đó dùng `PLATFORMIO_BUILD_DIR=/tmp/pio_build_ak_base_kit pio run -e app` thay vì sửa `platformio.ini`. Nếu môi trường không có PlatformIO/toolchain ARM (ví dụ session web), **không được báo là đã build thành công** — nói rõ là chưa build được.

Thành phẩm tự copy về `release/app/` và `release/boot/` (script `pio_copy_release.py`), tên file kèm version lấy từ `-DAPP_VERSION`.

## 3. Cấu trúc thư mục

```
ak-base-kit-pio/
├── platformio.ini              # SỰ THẬT DUY NHẤT về build: define module, include path, build_src_filter
├── boards/genericSTM32L151CB_bare.json
├── pio_build_flags.py          # cờ riêng C (-std=gnu99) / C++ (-std=gnu++11 -fno-rtti...) + cờ LINK
├── pio_copy_release.py         # post-action: copy .bin/.elf về release/<env>/
├── release/                    # firmware thành phẩm (commit vào repo)
├── docs/                       # tài liệu tiếng Việt (.md + .html)
└── sources/
    ├── application/            # firmware app @ 0x08003000
    │   ├── ak/{inc,src}/       # KERNEL AK — task, message, timer, fsm, tsm. KHÔNG sửa trừ khi thật cần
    │   ├── app/                # ★ CODE DỰ ÁN VIẾT Ở ĐÂY: task_*.cpp, task_list, app.cpp, shell.cpp
    │   │   └── screens/        # màn hình OLED (scr_*.cpp)
    │   ├── common/             # xprintf, cmd_line, screen_manager, view_render, container/{fifo,ring_buffer,log_queue}
    │   ├── driver/             # button, buzzer, eeprom, flash, gpio, led, Adafruit_oled_drv, AsyncDelay, nRF24
    │   ├── libraries/          # VENDOR: ArduinoJson, nlohmann, QRCode
    │   ├── networks/           # VENDOR: mbmaster-v2.9.6 (Modbus RTU), net/link (UART link), ArduinoZigBee
    │   ├── platform/stm32l/    # ★ CHỖ SỬA KHI ĐỔI BOARD: io_cfg.h/.c, sys_cfg.c, system.c, ak.ld, Libraries/ (SPL+CMSIS), arduino/
    │   └── sys/                # sys_boot (share data update FW), sys_dbg
    └── boot/                   # bootloader @ 0x08000000 — cấu trúc tương tự, rút gọn
        ├── app/                # boot_main() + uart_boot (giao thức nạp qua UART)
        └── ...
```

## 4. Kernel AK — quy tắc bắt buộc

Mỗi task là **một hàm** `void task_xxx(ak_msg_t* msg)` xử lý message theo `msg->sig`. Không có context switch, không có stack riêng, không preemption. Scheduler luôn lấy message của task có **mức ưu tiên cao nhất** đang chờ, chạy **run-to-completion**.

**Luật vàng — vi phạm là treo cả hệ thống:**

1. **Handler phải ngắn và return ngay.** Không `while(1)`, không delay dài, không busy-wait. Cần chờ → `timer_set(..., TIMER_ONE_SHOT)` rồi xử lý ở signal kế tiếp.
2. **Task giao tiếp với nhau chỉ bằng message** (`task_post_pure_msg` / `task_post_common_msg` / `task_post_dynamic_msg`). Không gọi hàm chéo giữa task, hạn chế biến toàn cục chia sẻ.
3. **Từ ISR post message phải nằm giữa** `task_entry_interrupt()` … `task_exit_interrupt()` (xem `systick_handler()` trong `platform/stm32l/system.c`).
4. **Mức ưu tiên** `TASK_PRI_LEVEL_0..7`, số càng lớn chạy càng trước. PRI_7 = `task_timer_tick` (đừng đặt task nghiệp vụ cao hơn), PRI_6 = `task_life` (nuôi watchdog), PRI_5..3 = link/network, PRI_4 = interface/dbg/display, PRI_2 = nghiệp vụ thường.
5. **Watchdog kép**: IWDG 32s + soft watchdog 20s, được nuôi trong `task_life` mỗi 1000 ms. Code chặn scheduler quá lâu sẽ bị reset.

### API kernel hay dùng

```c
task_post_pure_msg(des_task_id, sig);                       /* chỉ signal, pool 32 */
task_post_common_msg(des_task_id, sig, data, len);          /* len ≤ 64 byte (AK_COMMON_MSG_DATA_SIZE), pool 8 */
task_post_dynamic_msg(des_task_id, sig, data, len);         /* malloc, độ dài tùy ý, pool 8 */
timer_set(des_task_id, sig, duty_ms, TIMER_ONE_SHOT|TIMER_PERIODIC);   /* duty tính bằng ms (SysTick 1ms), pool 16 */
timer_remove_attr(des_task_id, sig);
task_remove_msg(task_id, sig);
```

Kích thước pool cấu hình bằng define trong `platformio.ini` (`AK_*_POOL_SIZE`), **không** sửa trong header. Pool nhỏ → tránh burst post nhiều message cùng lúc; message dài hơn 64 byte phải dùng dynamic msg.

### Signal

- `AK_SYS_DEFINE_SIG` = 0 (dành cho kernel/hệ thống), `AK_USER_DEFINE_SIG` = 10 — signal ứng dụng **phải** bắt đầu từ `AK_USER_DEFINE_SIG`.
- `sig` là `uint8_t` → tối đa 256 giá trị **trên mỗi task** (enum của mỗi task độc lập nhau, đều bắt đầu lại từ `AK_USER_DEFINE_SIG`).
- Enum signal + hằng số interval của tất cả task app nằm tập trung trong [`sources/application/app/app.h`](sources/application/app/app.h), chia theo block comment cho từng task.

## 5. Công thức thêm code

### Thêm một task mới (`task_sensor`)

1. **`app/task_list.h`** — thêm ID vào enum, **trước** `AK_TASK_EOT_ID`, thứ tự phải tăng dần; thêm `extern void task_sensor(ak_msg_t*);`.
2. **`app/task_list.cpp`** — thêm dòng vào `app_task_table[]`: `{AC_TASK_SENSOR_ID, TASK_PRI_LEVEL_2, task_sensor},` — **thứ tự dòng trong bảng phải khớp thứ tự enum**, và dòng `AK_TASK_EOT_ID` luôn ở cuối.
3. **`app/app.h`** — thêm block enum signal (`SENSOR_INIT = AK_USER_DEFINE_SIG, ...`) + các `#define ..._INTERVAL`.
4. **`app/task_sensor.cpp` + `.h`** — viết handler `switch (msg->sig)`. File mới trong `app/` **tự vào build** nhờ `+<application/app/*.cpp>`, không phải sửa `platformio.ini`.
5. **`app/app.cpp`** — kích hoạt: `task_post_pure_msg(AC_TASK_SENSOR_ID, SENSOR_INIT);` trong `app_task_init()`, hoặc `timer_set(...)` trong `app_start_timer()`.

Tham chiếu mẫu ngắn gọn nhất: [`app/task_life.cpp`](sources/application/app/task_life.cpp).

### Thêm lệnh shell

Trong [`app/shell.cpp`](sources/application/app/shell.cpp): khai báo `int32_t shell_xxx(uint8_t* argv);` → cài đặt hàm → thêm dòng vào `lgn_cmd_table[]` (giữ sentinel `{0,0,0}` ở cuối). Tham số lấy qua `str_parser_get_attr(n)`. In ra bằng `LOGIN_PRINT`.

### Thêm màn hình OLED

Trong `app/screens/`: tạo `scr_xxx.cpp/.h` theo mẫu [`scr_startup.cpp`](sources/application/app/screens/scr_startup.cpp) — một `view_dynamic_t` + `view_screen_t` + `void scr_xxx_handle(ak_msg_t*)`; khai báo `extern` trong `screens.h`. Chuyển màn bằng `SCREEN_TRAN(scr_xxx_handle, &scr_xxx)`, quay lại bằng `SCREEN_BACK()`. `task_display` chỉ làm một việc: `scr_mng_dispatch(msg)`.

### Đổi board / đổi chân

Sửa [`platform/stm32l/io_cfg.h`](sources/application/platform/stm32l/io_cfg.h) + `io_cfg.c` (chỗ sửa nhiều nhất), `sys_cfg.c` (clock 32 MHz HSI+PLL, console UART), `stm32l1xx_conf.h` (bật driver SPL nếu cần ngoại vi mới) — và nhớ thêm file `.c` SPL tương ứng vào `build_src_filter` (chỉ các file SPL được liệt kê mới được biên dịch).

## 6. Quy ước build (đọc kỹ trước khi sửa `platformio.ini`)

- **Cờ chung C+C++** → `build_flags` trong `platformio.ini`. **Cờ riêng theo ngôn ngữ và cờ LINK** → `pio_build_flags.py` (PlatformIO chỉ đưa `build_flags` vào `CCFLAGS`, `LINKCOM` không đọc `CCFLAGS`). `-nostartfiles` **bắt buộc** ở LINKFLAGS, nếu không sẽ lỗi `multiple definition of __dso_handle` do crtbegin.o.
- `-fno-rtti -fno-exceptions -fno-use-cxa-atexit` là bắt buộc — không có C++ runtime đầy đủ. **Không dùng exception, RTTI, `dynamic_cast`, hay STL cấp phát lớn.** Code hiện tại có include `<vector>/<map>/<deque>` nhưng thực tế dùng rất hạn chế; nhớ 16K RAM.
- **Include path trong `platformio.ini` tính từ thư mục gốc project**, không phải từ `src_dir`. `build_src_filter` thì ngược lại: **tính từ `src_dir = sources`**.
- Thêm thư mục source mới ⇒ phải thêm **cả** `-I...` (build_flags) **và** dòng `+<...>` (build_src_filter). Quên một trong hai là lỗi link hoặc lỗi include.
- Bật/tắt module bằng define trong `[env:app]`, kèm sửa `build_src_filter` tương ứng:

| Cờ | Module | Mặc định |
|----|--------|----------|
| `TASK_MBMASTER_EN` | Modbus master RTU (mbmaster) | Bật |
| `IF_LINK_UART_EN` | Link UART 3 tầng (phy/mac/link) | Bật |
| `SSD1309_DRIVER_EN` / `SH1106_DRIVER_EN` | OLED | SSD1309 |
| `USE_EXTERNAL_FLASH` | External SPI flash | Bật |
| `TASK_ZIGBEE_EN` | Zigbee | **Tắt** (`task_zigbee.cpp` bị loại trong `build_src_filter`) |
| `IF_NETWORK_NRF24_EN` | Mạng nRF24 | **Tắt** |
| `RELEASE`, `AK_IO_IRQ_ANALYZER`, `USING_USB_MOD`, `RTOS_DEV_EN` | — | Tắt (app); `RELEASE` bật ở boot |

- Log bật/tắt bằng `SYS_PRINT_EN` / `APP_PRINT_EN` / `APP_DBG_EN` / `SYS_DBG_EN` / `LOGIN_PRINT_EN` / `APP_DBG_SIG_EN` → macro trong [`app/app_dbg.h`](sources/application/app/app_dbg.h) (`APP_PRINT`, `APP_DBG`, `APP_DBG_SIG`, `LOGIN_PRINT`). Dùng các macro này, **không gọi thẳng `xprintf`** trong code app.

## 7. Quy ước code

- **Tab** để thụt lề (toàn bộ `sources/`), không dùng space. Dấu ngoặc `{` cùng dòng.
- Header guard kiểu `__TASK_LIST_H__`; header dùng được từ C++ phải bọc `#ifdef __cplusplus extern "C" { ... }`.
- Kernel `ak/src/` và driver hầu hết là **C** (`.c`), tầng app/screens/networks là **C++** (`.cpp`) — giữ nguyên phần mở rộng khi sửa, đổi `.c`→`.cpp` sẽ đổi cả cờ biên dịch lẫn linkage.
- Đặt tên: `task_*` (task), `AC_TASK_*_ID` (task id), `AC_*` / `<TASK>_*` (signal), `scr_*` (screen), `io_*` / `*_IO_PIN` (GPIO), `sys_*` (tầng system), `app_*` (dữ liệu/tiện ích app).
- Comment và tài liệu trong repo viết **tiếng Việt** (một số file cũ không dấu vì lịch sử Makefile) — giữ ngôn ngữ của file đang sửa.
- Header tác giả `@author: GaoKong` là từ base gốc, giữ nguyên khi sửa file cũ.

## 8. Bẫy đã biết

- **`Makefile.mk` rải khắp `sources/` là di sản của base gốc — PlatformIO KHÔNG dùng.** Thêm file mới không cần đụng vào chúng; nguồn sự thật là `platformio.ini`.
- Nhiều file tồn tại song song ở `application/` và `boot/` (`sys_boot.c`, `flash.c`, `led.c`, `xprintf.c`, `eeprom.cpp`, toàn bộ SPL). **Sửa một bên không tự áp dụng cho bên kia** — cân nhắc có cần sửa cả hai không, đặc biệt với `sys_boot` (định dạng share data phải khớp giữa boot và app, nếu lệch thì bootloader hiểu sai header firmware).
- `AK_TASK_EOT_ID` / `AK_TASK_POLLING_EOT_ID` là sentinel kết thúc bảng — luôn giữ ở cuối enum và cuối bảng.
- Polling task (`task_polling_console`, `task_polling_zigbee`) chỉ chạy khi **mọi hàng đợi message rỗng** — không đặt logic quan trọng về thời gian ở đó.
- `.non_clear_ram` (`app_non_clear_ram.cpp`) giữ dữ liệu qua soft-reboot để phân tích lỗi — không tự động khởi tạo về 0, chỉ reset khi power-on reset (`app_power_on_reset()`).
- `sys_irq_timer_10ms()` trong `app.cpp` được gọi từ `systick_handler()` mỗi 10 ms để polling button — đây là **ngữ cảnh ISR**, giữ cực ngắn.
- Bản đồ external flash của app nằm trong [`app/app_flash.h`](sources/application/app/app_flash.h) (log fatal, dump RAM, vùng firmware `0x80000`) — đổi giá trị phải đồng bộ với bootloader.
- **Không sửa** `platform/stm32l/Libraries/` (SPL + CMSIS), `libraries/` (ArduinoJson, nlohmann, QRCode), `networks/mbmaster-v2.9.6/`, `networks/ArduinoZigBee/` — vendor code. Cần thay đổi hành vi thì bọc thêm lớp ở `driver/` hoặc port layer.
- `release/*.bin/.elf` được commit; chỉ cập nhật khi thực sự build ra bản mới và **đã tăng `-DAPP_VERSION`** ở cả `[env:app]` và `[env:boot]`.
- Còn sót file rác `sources/boot/platform/stm32l/.system.c.swp` trong repo — bỏ qua, đừng đọc/sửa.
- Khi copy base sang dự án mới, đổi: `APP_TITLE`, `APP_VERSION`, `build_dir`, và prefix `ak_base_kit_` trong `pio_copy_release.py`.

## 9. Git

- Nhánh mặc định: `main`. Commit message ngắn gọn, tiếng Việt hoặc tiếng Anh đều được (lịch sử hiện tại: `init`, `update docs`).
- Không commit thư mục build (`.pio/`, `.cache/`, `compile_commands.json`, `*.o/.d/.su` — đã có trong `.gitignore`).
