# Modbus RTU trên RS485

Dùng thư viện [nanoMODBUS](https://github.com/debevv/nanoMODBUS) (MIT, `third_party/nanomodbus/`).
Một cổng RS485, một vai trò, chọn lúc build:

| Vai trò | PlatformIO | CMake | Cờ |
|---|---|---|---|
| Slave (mặc định) | `pio run -e app` | `-DAK_MODBUS=slave` | `APP_MODBUS_SLAVE` |
| Master | `pio run -e app_mbmaster` | `-DAK_MODBUS=master` | `APP_MODBUS_MASTER` |
| Không dùng | — | `-DAK_MODBUS=none` | — |

Cổng: USART2, PA2 TX, PA3 RX, chân hướng PA1 (`port/stm32l151/port_cfg.h`), 8N1.
Tốc độ và địa chỉ: `APP_MB_BAUD` (9600), `APP_MB_UNIT_ID` (1) trong `app/app.h`.

> **Đã kiểm trên AK Base Kit (06/10/2026)** qua bộ chuyển USB-RS485:
> slave đọc/ghi thanh ghi và trả đúng exception; OTA qua RS485 (ảnh 20K mất 26 s, ảnh 27K mất 35 s ở 9600 baud);
> bỏ dở giữa chừng rồi nạp lại; master đọc/ghi một slave giả trên máy tính, nhận đúng exception và timeout.
> Unit test trên máy tính: 174 kiểm tra.

## Slave

App khai thanh ghi thành các khối `uint16_t` của chính nó:

```c
static uint16_t status[3], setpoint[2];

static const mb_reg_block_t holding[] = {
	{ 0,   3, status,   MB_REG_RO },
	{ 100, 2, setpoint, MB_REG_RW },
};
static const mb_slave_map_t map = { holding, 2, 0, 0, on_write };

mb_slave_init(1, 9600, &map);        /* địa chỉ 1 */
mb_slave_enable_ota(on_ota_commit);  /* cho phép cập nhật firmware qua RS485 */
/* gọi mb_slave_poll() trong một polling task */
```

- Mã hàm: 03, 04, 06, 16.
- Yêu cầu phải nằm trọn trong một khối, nếu không trả exception 2. Ghi vào khối chỉ đọc cũng trả exception 2.
- `on_write(address, quantity)` được gọi sau khi master ghi xong.
- `mb_slave_poll()` trả về ngay khi đường truyền rảnh; khi một khung đã bắt đầu thì nó ở lại tới lúc trả lời xong
  (khoảng 1 ms mỗi byte ở 9600).

Bảng thanh ghi của app mẫu:

| Địa chỉ | R/W | Nội dung |
|---|---|---|
| 0 | R | Phiên bản app: `(major << 8) \| minor` |
| 1 | R | Phiên bản app: patch |
| 2 | R | Uptime, giây |
| 3 | R | Số bản ghi sự cố |
| 16–19 | R/W | Thanh ghi thử, giữ tới khi reset |

Lệnh shell `mb` in số byte đã nhận và số request đã trả lời. Có byte mà không có request nào được trả lời thì sai
baud hoặc đảo A/B; không có byte nào thì không có tín hiệu tới bộ thu.

## Cập nhật firmware qua RS485

Khối thanh ghi `0xF000`, cùng địa chỉ và trình tự với base cũ, mang ảnh `.img` của base mới.

| Thanh ghi | R/W | Ý nghĩa |
|---|---|---|
| `F000` | W | Lệnh: 1 BEGIN, 2 COMMIT, 3 ABORT |
| `F001` | R | Trạng thái: 0 rảnh, 1 đang nhận, 2 đã commit; `0x8001` header, `0x8002` offset, `0x8003` CRC ảnh, `0x8004` kích thước |
| `F002–F003` | R/W | Kích thước cả file `.img` (hi, lo) |
| `F004` | R/W | Không dùng với kiểu ảnh này (ghi 0) |
| `F005–F006` | R/W | Kiểu ảnh: `0x57464B41` |
| `F007–F008` | R | Số byte đã nhận |
| `F009` | R | Mã lỗi chi tiết của lần hỏng gần nhất (`fw_err_t`) |
| `F010–F011` | W | Offset của khối, ghi cùng dữ liệu |
| `F012–F051` | W | Dữ liệu khối, tối đa 64 thanh ghi = 128 byte, byte cao trước |

- COMMIT kiểm cả ảnh trong STAGING (CRC header, tên board, địa chỉ nạp, CRC ảnh, bảng vector) rồi mới trả lời.
  Trước đó app đang chạy không bị đụng tới; đứt giữa chừng thì chạy lại từ BEGIN.
- Khối hoặc COMMIT gửi lại vì mất phản hồi đều được chấp nhận, không ghi hai lần.
- Broadcast (địa chỉ 0) không điều khiển được khối này.
- Ảnh kiểu base cũ (`0x1A2B3C4D`) bị từ chối với trạng thái `0x8001`.

Tool trên máy tính (chỉ cần `pyserial`):

```bash
python tools/ak_mb.py --port COM16 info
python tools/ak_mb.py --port COM16 read 16 4
python tools/ak_mb.py --port COM16 write 16 123 456
python tools/ak_mb.py --port COM16 flash .pio/build/app/app.img
```

## Master

```c
mb_master_init(9600, 200);                           /* chờ trả lời tối đa 200 ms */
int err = mb_master_read_holding(5, 0, 4, regs);     /* thiết bị 5, thanh ghi 0..3 */
```

Giá trị trả về: `0` thành công, số dương là mã exception của slave, số âm là lỗi đường truyền (`MB_ERR_TIMEOUT`…).
Mỗi lần gọi chặn trong thời gian khung cộng thời gian chờ trả lời, nên gọi một request cho mỗi message và giữ
timeout ngắn. Lệnh shell: `mb read <unit> <addr> [count]`, `mb write <unit> <addr> <value>`.
