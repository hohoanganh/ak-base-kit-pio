# Lịch sử thay đổi

Dự án mới luôn clone theo **tag mới nhất** và ghi version base vào README của mình — đó là cách duy
nhất biết sau này dự án đang thiếu bản sửa nào. Bootloader có số version riêng (`BOOT_VER`, in ra
console lúc khởi động).

Tổng hợp tính năng v1.2.0 – v1.3.0 (Modbus mới + OTA qua RS485):
[docs/tinh-nang-moi-v1.2-v1.3.md](docs/tinh-nang-moi-v1.2-v1.3.md).

## ak-mcu-base — sau v1.3.3, chưa phát hành

- **Lệnh `ui open <tên>`**: mở thẳng một màn hình theo một phần tên trong menu (`ui open tetris`, `ui open 3d`,
  `ui open menu`), không phải bấm qua menu. Dùng được trên kit qua shell.
- **Trang chạy thử sống ngay khi mở:** kit tự đi một vòng qua các màn hình đẹp nhất cho tới khi có người bấm nút;
  hàng nút tắt tới từng màn hình; link chia sẻ dạng `play/?screen=tetris`.
- **Trang [hai kit chơi Pong](https://hohoanganh.github.io/ak-base-kit-pio/play/pong.html):** hai bản firmware cạnh nhau trên một trang, nối bằng đường
  RS485 giả lập; xem được ai làm chủ, số byte qua lại, và rút dây để thấy chúng tự quay về chơi một mình.
- **`port/web/kit-ui.js`**: một kit thành một thành phần nhúng được vào trang bất kỳ; trang giới thiệu của dự án
  giờ có kit chạy thật ngay đầu trang.
- **README**: GIF 19 giây các cảnh đẹp nhất (`tools/highlights_gif.py`), nút chạy thử, bảng lối vào;
  thêm [README.en.md](README.en.md) tiếng Anh.

## ak-mcu-base v1.3.3 — 06/10/2026

App **1.3.3**, bootloader không đổi (1.2.0). Mã firmware không đổi so với v1.3.2 ngoài số phiên bản; bản này thêm clip mẫu và tool.

- **Clip mẫu của màn hình Video là logo AK Foundation xoay 3D** (6 giây, 19 KB), thay cho clip đốm trắng; bản chạy trên
  trình duyệt cũng dùng clip này. Logo thuộc về AK Foundation.
- **`tools/spin_clip.py`**: biến một logo bất kỳ (ảnh trên nền trơn) thành clip `.akv` xoay 3D: tấm có bề dày, mặt trước
  trắng, cạnh bên tô dither, mặt sau cũng đọc được.

## ak-mcu-base v1.3.2 — 06/10/2026

App **1.3.2**, bootloader không đổi (1.2.0).

- **Pong qua RS485** (màn hình mới của demo): hai kit nối chung dây RS485 chơi với nhau; kit có số ngẫu nhiên lớn hơn
  làm chủ, tính đường bóng và gửi trạng thái 20 lần/giây, kit kia trả lời vị trí thanh đỡ. Một mình thì kit tự chơi
  với máy và phát "hello" chờ kit thứ hai. Khi màn hình này mở, cổng RS485 thuộc về trò chơi, Modbus tạm nghỉ.
  `tools/pong_peer.py` cho máy tính đóng vai kit thứ hai qua USB-RS485. Đã thử trên kit với máy tính ở cả hai vai:
  20 khung/giây, 300/300 khung được trả lời, không khung hỏng; rời màn hình thì Modbus trả lời lại bình thường.
- **Xem màn hình kit trên máy tính:** lệnh shell `ui dump` (một lần) và `ui stream` (liên tục) gửi các trang màn hình
  đã đổi dưới dạng dòng chữ nén PackBits, có CRC. `tools/ak_screen.py` chụp ảnh (`shot`), quay GIF (`record`) hoặc mở
  cửa sổ xem trực tiếp (`live`), bấm phím 1 2 3 thay cho ba nút. Đo trên kit: 19 hình/giây với màn hình đồng hồ,
  9,5 hình/giây khi cả màn hình đổi liên tục.
- **Bản chạy trên trình duyệt** (<https://hohoanganh.github.io/ak-base-kit-pio/play/>): firmware (kernel, shell, Modbus slave, bộ demo) biên dịch sang
  WebAssembly bằng Emscripten. `port/web/web_main.c` đóng vai bo mạch, `port/host` đóng vai chip; trang web có màn hình,
  ba nút (chuột, chạm, phím 1 2 3), còi, console gõ lệnh shell, nút gây FATAL để xem nhật ký sự cố, nạp clip `.akv`.
  Mở trang ở hai tab là hai bản firmware chơi Pong với nhau: byte RS485 đi giữa hai tab. Dựng lại bằng
  `python port/web/build.py`; CI có thêm bước build này. `app_main()` tách ra `app_init()` để port không có vòng lặp
  vô hạn gọi được `task_run_once()`.
- **Máy hiện sóng mini** (màn hình Scope): đồ thị cuộn 120 điểm, tự co giãn. Số liệu vào bằng lệnh shell
  `plot <số>` hoặc ghi thanh ghi Modbus 16. `tools/ak_plot.py` gửi sóng sin, nhiễu, hoặc số đọc từ stdin, qua UART
  hay RS485. Đã thử cả hai đường trên kit.
- Tổng kết phiên làm việc 06/10/2026: [ak-mcu-base/docs/tong-ket-2026-10-06.md](ak-mcu-base/docs/tong-ket-2026-10-06.md).
- Unit test demo: 456 kiểm tra.

## ak-mcu-base v1.3.1 — 06/10/2026

App **1.3.1**, bootloader không đổi (1.2.0). Bản này chủ yếu thêm bộ demo cho AK Base Kit.

- **Ba demo mới trên kit:** Dino runner (B1 nhảy, B2 cúi), khối 3D quay (lập phương, bát diện, kim tự tháp; khung dây
  hoặc tô bóng bằng dither, chỉ dùng số nguyên), máy hát RTTTL trên còi với 6 bài, các nốt chạy trên màn hình.
  Menu cuộn được, 7 mục. `gfx` thêm `gfx_line`, `gfx_tri` (tô theo mức xám), `gfx_bitmap`. Unit test demo: 90 kiểm tra.
  Đo trên kit: Dino tối đa 24 ms mỗi khung, 3D 34–37 ms, máy hát 4 ms.
- **Thêm ba demo nữa:**
  - *Mê cung 3D* (ray casting kiểu Wolfenstein, chỉ dùng số nguyên): B1/B2 quay, B3 đi/dừng, tìm cửa ra có sọc.
    Đo trên kit: tối đa 37 ms mỗi khung khi đang đi.
  - *Video từ flash SPI*: clip 1 bit 128×64 nằm ở vùng 0–512 KB của flash SPI (vùng OTA không dùng), chỉ lưu các
    trang thay đổi, nén PackBits, có thể lưu dạng hiệu với khung trước. Tool `tools/ak_video.py` chuyển video, GIF,
    thư mục ảnh thành `.akv` và nạp vào kit qua UART (57 KB trong 8 s). Đo trên kit: 41–47 ms mỗi khung khi đổi cả màn hình.
  - *Trạm thời tiết*: nhiệt độ, độ ẩm từ SHT45, đồ thị 96 điểm (mỗi 1 s, 1 phút hoặc 15 phút = 24 giờ); cảm biến được đọc
    mỗi giây dù đang mở màn hình nào. Lệnh shell `th`, `th csv`.
  Menu 10 mục. Unit test demo: 124 kiểm tra.
- **Thêm ba game và bộ màn hình chờ:** Tetris (giếng 10×20, B1/B2 sang ngang, B3 xoay, giữ B1+B2 để thả), Breakout,
  Invaders, và màn hình chờ gồm Game of Life (lưới 64×32, đếm hàng xóm bằng phép bit trên từ 64 bit), trường sao, plasma.
  Cả ba game đều tự chơi được với `ui auto`. Menu 14 mục. Unit test demo: 137 kiểm tra.
  Đo trên kit: Tetris 20 ms mỗi khung, Breakout 15 ms, Invaders 31 ms, màn hình chờ 45–50 ms.
- **Timer khung của demo đặt lại sau mỗi khung** thay cho timer chu kỳ: hiệu ứng đổi cả màn hình cần hơn 50 ms thì chỉ
  chạy chậm đi, không dồn message vào pool.
- **Giao thức nạp (`fw_proto`) có chỗ mở rộng:** lệnh từ `0x40` trở lên được chuyển cho hàm `ext` của ứng dụng
  (demo dùng để ghi file vào flash SPI). Bootloader không đổi hành vi.
- **`demo/kit.h`** thêm SHT45 (`kit_sht_start/read`) và kho file trên flash SPI (`kit_store_*`).
- **Demo tự chơi:** lệnh shell `ui auto` cho các game và mê cung tự chạy, máy hát tự phát lần lượt; bấm nút là
  giành lại quyền điều khiển.
- **Ảnh động cho tài liệu:** `tests/test_demo <thư mục> record` ghi từng khung từ mã vẽ thật, `tools/demo_gif.py`
  dựng thành GIF (README, [ak-mcu-base/docs/demo-kit.md](ak-mcu-base/docs/demo-kit.md)).
- **Huy hiệu Repo Traffic** ở đầu README.
- **Ý tưởng demo tiếp theo**, kèm nguồn tham khảo: [ak-mcu-base/docs/y-tuong-demo.md](ak-mcu-base/docs/y-tuong-demo.md).

## ak-mcu-base v1.3.0 — 06/10/2026

App **1.3.0**, bootloader không đổi (1.2.0).

- **Modbus RTU trên RS485** (nanoMODBUS): slave với bảng thanh ghi khai theo khối, master, OTA qua khối thanh ghi
  `0xF000` mang ảnh `.img` của base mới; tool `tools/ak_mb.py`. Vai trò chọn lúc build (`env:app` là slave,
  `env:app_mbmaster`). Unit test 174 kiểm tra. Đã kiểm trên kit: đọc/ghi thanh ghi, OTA qua RS485 (20K trong 26 s ở
  9600 baud), master.
  Tài liệu: [ak-mcu-base/docs/modbus.md](ak-mcu-base/docs/modbus.md).
- **Demo trên AK Base Kit** (`env:demo`): menu, đồng hồ số (RTC hoặc tự đếm), Snake, Flappy, màn hình hệ thống trên
  OLED 128×64 + 3 nút + còi; điều khiển được qua shell (`ui`). Đã chạy trên kit; ảnh màn hình dựng từ mã vẽ thật.
  Tài liệu: [ak-mcu-base/docs/demo-kit.md](ak-mcu-base/docs/demo-kit.md).

## ak-mcu-base v1.2.0 — 06/10/2026

Bootloader **1.2.0**. Từ bản này `ak-mcu-base` là base cho các dự án mới.

- **Tạo dự án mới:** `tools/new_project.py` xuất một dự án độc lập (base + SPL/CMSIS + file board), ghi `BASE_VERSION`,
  đặt tên board riêng cho sản phẩm.
- **Build PlatformIO chạy được** (trước đó hỏng ngay bước đầu: script build chạy sai thời điểm). Ảnh build bằng
  PlatformIO đã nạp và OTA trên board.
- **Nhật ký sự cố** trong EEPROM (8 bản ghi): HardFault, FATAL, watchdog, task bị bỏ đói; kèm task và signal đang
  chạy. Lệnh shell `crash`, `crash clear`, `crash test …`. **Cần bootloader từ 1.2.0.**
- **Giám sát task:** `task_system` ping mọi task mỗi giây; task không được chạy 3 nhịp thì ghi nhật ký và reset.
  Kernel thêm `task_alive_take()`, `AK_SIG_PING`, và báo port handler sắp chạy (`ak_port_note_dispatch`).
- **Đo mức dùng stack:** lệnh `stat` in số byte RAM chưa từng dùng.
- **CI:** GitHub Actions chạy test host và build STM32, giới hạn bootloader 10.240 B.
- **NVM:** `HAL_NVM_SIZE` tăng lên 256; `boot_ctrl` giữ nguyên chỗ (hai bản ghi 32 B ở đầu).
- Tài liệu: [ak-mcu-base/docs/tien-ich.md](ak-mcu-base/docs/tien-ich.md) ·
  [ak-mcu-base/docs/huong-toi-uu-tiep.md](ak-mcu-base/docs/huong-toi-uu-tiep.md) (các hướng tra cứu được, kèm nguồn).

## ak-mcu-base v1.1.0 — 06/10/2026

Thư mục [`ak-mcu-base/`](ak-mcu-base/README.md) đánh số riêng (tag `ak-mcu-base-v<x.y.z>`), không ảnh hưởng
base trong `sources/`. Bản đầu tiên chạy trên board thật; bootloader **1.1.0**.

- **Kernel:** timer mềm dùng mốc hết hạn tuyệt đối (không trôi, không nổ sớm, tick chỉ post khi đến hạn);
  ngoài task thì id hiện tại là `AK_TASK_IDLE_ID`; vòng chính kiểm hàng đợi và `WFI` trong một critical section.
- **Build:** LTO bật mặc định, boot và app nhỏ hơn khoảng 14%.
- **App:** console TX qua ring + ngắt TXE; shell có `info` (kiểm header) và `verify` (CRC toàn ảnh); dồn lệnh
  không còn gây FATAL.
- **Bootloader 1.1.0:** `boot_ctrl` hai bản ghi luân phiên, watchdog 10 s, vòng chờ flash SPI có giới hạn.
  **App từ 1.1.0 phải đi với bootloader từ 1.1.0.**
- **Tool:** `ak_fw.py` OTA ảnh 10,5K từ 5,4 s xuống 1,6 s.
- **So với base cũ:** [ak-mcu-base/docs/so-voi-base-cu.md](ak-mcu-base/docs/so-voi-base-cu.md).
- README gốc viết lại gọn; các mục dài chuyển sang [docs/bat-dau-du-an-moi.md](docs/bat-dau-du-an-moi.md) và
  [docs/ota-rs485.md](docs/ota-rs485.md).

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

- **Đã kiểm trên board thật (25/09/2026):** OTA 1.3.0 → 1.3.1 → 1.3.0 qua USB-RS485 @9600
  ≈ 78 s/lượt, bootloader xóa 0,8 s + chép 2,7 s; cắt ngang giữa chừng board vẫn chạy app cũ và
  chạy lại được; 200/200 lần đọc FC03 đúng. Chi tiết + các ca thử âm:
  [docs/tinh-nang-moi-v1.2-v1.3.md](docs/tinh-nang-moi-v1.2-v1.3.md) mục 5.
- **Công cụ PC:** `python -m epcb_applib.ota` (epcb-applib ≥ 1.4.0) — kiểm bảng vector ảnh, tự
  ABORT phiên dở, xác nhận phiên bản sau khi board khởi động lại.
- **Nạp bằng ST-Link:** nếu `pio run -t upload` báo lỗi `hla_swd` (OpenOCD mới + ST-Link V2), dùng
  `ST-LINK_CLI.exe` — lệnh ở tài liệu trên, mục 4.

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
- **Repo:** thôi đưa file `.elf` trong `release/` vào git (~1,1 MB mỗi bản, repo phình theo từng
  bản). Git chỉ giữ `.bin`; `.bin` + `.elf` của các bản đính kèm ở
  [GitHub Releases](https://github.com/hohoanganh/ak-base-kit-pio/releases).
- **Tài liệu:** hai guide EPCB trong repo MCP (`epcb-platformio-build`, `epcb-start-project`) cập
  nhật theo v1.1.2.
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
