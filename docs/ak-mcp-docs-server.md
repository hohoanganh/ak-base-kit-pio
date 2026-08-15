# AK MCP Docs Server — tài liệu kernel AK cho AI assistant

Repo này tích hợp [mcp-docs-server](https://github.com/the-ak-foundation/mcp-docs-server)
của AK Foundation — MCP server cung cấp tài liệu kernel AK (Active Kernel) dạng
tra cứu được cho Claude Code / Cursor / Copilot. Khi làm việc trong repo này,
AI assistant tự có các tool tra API kernel, guide viết task/driver, và phân
tích log UART — trả lời dựa trên tài liệu chính thức thay vì đoán.

## Tool được cung cấp

| Tool | Công dụng |
|---|---|
| `search_ak_docs` | Tìm kiếm toàn bộ tài liệu (BM25) |
| `get_ak_api` / `list_ak_api` | Tra chữ ký hàm, tham số, ngữ nghĩa theo module |
| `get_ak_guide` | Recipe viết task, driver, screen, debug |
| `analyze_ak_log` | Chẩn đoán log UART (lỗi, timing) |
| `decode_ak_lcd` | Dựng lại framebuffer dump thành text/PNG |
| `start_ak_project` | Lấy release base-kit mới nhất kèm lệnh setup |

Prompt mẫu: `ak-new-project`, `ak-new-task`, `ak-new-driver`, `ak-debug`.

## Cài đặt trên máy mới

Gói `ak-mcp` **chưa publish lên npm** (README gốc ghi `npx -y ak-mcp` nhưng
registry trả 404 — kiểm tra 08/2026), nên phải clone về build thủ công.
Cần Node.js ≥ 20.

```powershell
# Clone NGOAI OneDrive (node_modules hang nghin file nho, de OneDrive sync la hong)
cd D:\dev
git clone --depth 1 https://github.com/the-ak-foundation/mcp-docs-server.git
cd mcp-docs-server
npm install
npm run build
```

File [.mcp.json](../.mcp.json) ở gốc repo trỏ tới bản build này:

```json
{
  "mcpServers": {
    "ak-docs": {
      "command": "node",
      "args": ["D:\\dev\\mcp-docs-server\\dist\\cli\\bin.js"]
    }
  }
}
```

**Lưu ý:** đường dẫn trong `.mcp.json` là tuyệt đối theo máy — máy khác cài ở
chỗ khác thì sửa lại dòng `args` cho khớp. Claude Code sẽ hỏi xác nhận lần đầu
mở repo (project-scoped MCP server).

## Kiểm tra hoạt động

```powershell
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' | node D:\dev\mcp-docs-server\dist\cli\bin.js
```

Trả về JSON có `"serverInfo":{"name":"ak-mcp"}` là server chạy đúng.

## Cập nhật tài liệu

Tài liệu kernel được vendor sẵn trong repo mcp-docs-server (snapshot theo tag,
mặc định v1.3 của ak-base-kit-stm32l151). Khi AK Foundation cập nhật:

```powershell
cd D:\dev\mcp-docs-server
git pull
npm run build
```

**Lưu ý phạm vi:** tài liệu server phục vụ là của **ak-base-kit-stm32l151 bản
gốc** (build Makefile). Repo này đã chuyển sang PlatformIO và có chỉnh sửa
riêng (BSF, bootloader map, pio_*.py) — phần kernel AK (task, message, timer,
fsm) dùng chung nên tra cứu vẫn đúng, nhưng phần build/flash thì theo tài liệu
của repo này, không theo hướng dẫn Makefile của server.
