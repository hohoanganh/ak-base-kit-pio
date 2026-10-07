# Cập nhật firmware qua RS485 (Modbus)

Áp dụng cho base trong `sources/` từ v1.3.0, env `app_mbslave`. Tổng hợp tính năng và kết quả kiểm trên board: [tinh-nang-moi-v1.2-v1.3.md](tinh-nang-moi-v1.2-v1.3.md).

Env `[env:app_mbslave]` (Modbus SLAVE trên USART2) nhận firmware mới qua RS485 và tự nạp vào
vùng App — không cần ST-Link. State machine nằm ở khối thanh ghi holding `0xF000`, bảng thanh ghi
+ luật đầy đủ ở
[docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md](../../docs/superpowers/specs/2026-09-25-nanomodbus-va-ota-modbus-design.md):

| Thanh ghi | Đọc/ghi | Ý nghĩa |
|---|---|---|
| `0xF000` | W | CMD: 1 = BEGIN, 2 = COMMIT, 3 = ABORT |
| `0xF001` | R | STATUS: 0 idle, 1 đang nhận, 2 đã commit (sắp reset), 0x8001/0x8002/0x8003/0x8004 = lỗi header/offset/checksum/quá cỡ |
| `0xF002–0xF003` | R/W | `bin_len` (hi, lo) — **phải là bội số của 4** |
| `0xF004` | R/W | checksum |
| `0xF005–0xF006` | R/W | psk (hi, lo) — phải khớp `FIRMWARE_PSK` |
| `0xF007–0xF008` | R | số byte đã nhận (hi, lo) |
| `0xF010–0xF011` | W | offset khối (hi, lo) |
| `0xF012–0xF051` | W | dữ liệu khối, tối đa 64 thanh ghi = 128 byte |

Ghi chú khi dùng:

- File `.bin` được **đệm thêm byte `0xFF` cho tròn bội số 4** ở phía công cụ PC trước khi truyền —
  slave không tự làm tròn.
- Đứt kết nối giữa chừng (trước khi CMD = COMMIT thành công): app cũ vẫn chạy bình thường, internal
  flash chưa bị đụng tới — cứ BEGIN lại từ đầu.
- Sau khi COMMIT thành công, state machine **khoá lại** (chỉ còn STATUS đọc được) và thiết bị tự
  reset sau ~200 ms để bootloader nạp bản mới.
- Phải gọi đích danh **địa chỉ slave** (unit_id) của từng board — **broadcast (unit_id 0) bị chặn
  hoàn toàn**, không có phản hồi nên không dùng để OTA được.
- 🔴 **Chỉ được gửi ảnh build `app_mbslave`**, đúng mẫu tên file
  `release/app_mbslave/ak_base_kit_app_mbslave_v<x.y.z>.bin`. Gửi nhầm ảnh `env:app` (master) vẫn
  qua được kiểm tra vector bảng (cả hai env dùng chung layout app) nhưng ảnh đó **không có task
  Modbus slave** — nạp xong board mất luôn đường OTA qua RS485, phải quay lại nạp bằng ST-Link.
- Công cụ PC (`epcb_applib.ota`) tự kiểm anh là vector bảng app hợp lệ trước khi gửi (từ chối
  `.elf`/`.hex`/ảnh bootloader) và tự xác nhận phiên bản qua Modbus sau khi thiết bị khởi động lại:

  ```bash
  python -m epcb_applib.ota COMx release/app_mbslave/ak_base_kit_app_mbslave_v1.3.0.bin --slave 1 --baud 9600
  ```

## Tool nạp bằng giao diện: EPCB Modbus Flash

Không cần cài Python — tải một file exe chạy thẳng trên Windows 10/11:
[**tools/EPCB_Modbus_Flash_v1.0.0.exe**](../tools/EPCB_Modbus_Flash_v1.0.0.exe) (≈ 13 MB).

1. Cắm bộ chuyển USB-RS485 vào board, mở exe.
2. Chọn cổng COM của bộ chuyển, baud `9600`, slave `1` → bấm **Kiểm tra board** (hiện phiên bản
   firmware đang chạy).
3. **Chọn file .bin** — lấy file `release/app_mbslave/ak_base_kit_app_mbslave_v<x.y.z>.bin`. Tool
   kiểm file ngay khi chọn và khóa nút nạp nếu file sai loại.
4. Bấm **⚡ NẠP FIRMWARE** (≈ 80 s cho ảnh 58 KB ở 9600 baud). Xong sẽ hiện **PASS / FAIL**, tool tự
   đọc lại phiên bản sau khi board khởi động lại. Bấm **Hủy** giữa chừng thì board vẫn chạy app cũ.

Mỗi lần nạp ghi một dòng vào log Excel trong thư mục `logs\` cạnh exe. Lần đầu mở, Windows có thể
cảnh báo "Windows protected your PC" (exe chưa ký số) → **More info** → **Run anyway**.
