# AK MCP Docs Server — tài liệu kernel AK cho AI assistant

Repo này tích hợp mcp-docs-server của AK Foundation — MCP server cung cấp tài
liệu kernel AK (Active Kernel) dạng tra cứu được cho Claude Code / Cursor /
Copilot. Khi làm việc trong repo này, AI assistant tự có các tool tra API
kernel, guide viết task/driver, và phân tích log UART — trả lời dựa trên tài
liệu chính thức thay vì đoán.

Dùng **fork của EPCB**, nhánh `epcb`:
[hohoanganh/mcp-docs-server](https://github.com/hohoanganh/mcp-docs-server/tree/epcb)
(upstream: [the-ak-foundation/mcp-docs-server](https://github.com/the-ak-foundation/mcp-docs-server)).

**Vì sao phải fork:** corpus gốc viết cho bản build Makefile của
`ak-base-kit-stm32l151`, một số guide chỉ sai cho repo PlatformIO này — ví dụ
guide `agent-workflow` bảo sửa `RELEASE_OPTION` trong `application/Makefile`,
file đó không tồn tại ở đây. Nhánh `epcb` thêm 2 guide riêng:

| Guide | Nội dung |
|---|---|
| `epcb-platformio-build` | build/nạp bằng `pio`, RELEASE ở `platformio.ini`, **bắt buộc seed BSF sau khi nạp SWD**, cảnh báo không được bỏ cờ linker `max-page-size=4` |
| `epcb-start-project` | khởi tạo sản phẩm mới từ source base theo tag, ghi lại version base |

Phần kernel AK không đổi nên toàn bộ guide còn lại của upstream vẫn dùng nguyên.

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

**Bắt buộc clone về build thủ công** — không có cách cài nhanh:

- `npx -y ak-mcp` (như README gốc ghi): gói **chưa publish lên npm**, registry
  trả 404 (kiểm tra 08/2026).
- `npx github:the-ak-foundation/mcp-docs-server`: cũng không chạy — repo
  gitignore cả `dist/` lẫn `generated/corpus.json` và không có script
  `prepare`, nên cài từ git xong vẫn không có gì để chạy.

Cần Node.js ≥ 22.6 (theo `engines` trong `package.json`).

```powershell
# Clone NGOAI OneDrive (node_modules hang nghin file nho, de OneDrive sync la hong)
cd D:\dev
git clone -b epcb https://github.com/hohoanganh/mcp-docs-server.git
cd mcp-docs-server
git remote add upstream https://github.com/the-ak-foundation/mcp-docs-server.git
npm install
npm run build
```

Nhánh mặc định phải là `epcb` — nhánh `main` giữ sạch để đồng bộ upstream.

File [.mcp.json](../.mcp.json) ở gốc repo trỏ tới bản build này:

```json
{
  "mcpServers": {
    "ak-docs": {
      "command": "node",
      "args": ["${AK_MCP_HOME:-D:/dev/mcp-docs-server}/dist/cli/bin.js"]
    }
  }
}
```

Repo này nằm trên GitHub và được clone về nhiều máy, nên đường dẫn **không**
hard-code: `.mcp.json` dùng cú pháp biến môi trường của Claude Code
(`${VAR:-default}`). Máy nào cài server đúng `D:\dev\mcp-docs-server` thì chạy
luôn không cần làm gì; máy cài chỗ khác chỉ cần đặt biến `AK_MCP_HOME`:

```powershell
# Windows - dat vinh vien cho user (mo terminal moi de co hieu luc)
setx AK_MCP_HOME "E:\tools\mcp-docs-server"
```

```bash
# Linux/macOS - them vao ~/.bashrc hoac ~/.zshrc
export AK_MCP_HOME="$HOME/dev/mcp-docs-server"
```

Claude Code sẽ hỏi xác nhận lần đầu mở repo (project-scoped MCP server).

## Kiểm tra hoạt động

```powershell
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' | node D:\dev\mcp-docs-server\dist\cli\bin.js
```

Trả về JSON có `"serverInfo":{"name":"ak-mcp"}` là server chạy đúng.

## Cập nhật từ upstream

Tài liệu kernel được vendor sẵn trong repo mcp-docs-server (snapshot theo tag
của ak-base-kit-stm32l151). Khi AK Foundation cập nhật, merge vào nhánh `epcb`:

```powershell
cd D:\dev\mcp-docs-server
git fetch upstream
git checkout main; git merge --ff-only upstream/main; git push origin main
git checkout epcb; git merge main
npm run build        # bat buoc: corpus.json bi gitignore, khong tu sinh lai
npm test             # 45 test
```

## Thêm guide EPCB mới

Guide chỉ là markdown + frontmatter trong `corpus/guides/`, thêm file rồi
`npm run build` là xong (`id` tự lấy từ tên file nếu không khai báo):

```markdown
---
id: epcb-ten-guide
title: "EPCB: tieu de"
section: guide
tags: epcb, tu, khoa, tim, kiem
summary: Mot dong tom tat - hien trong ket qua search.
---

# Nội dung...
```

Sau khi build, kiểm tra bằng `npm run drift` (kiểm tra tham chiếu chéo) và
`npm test`. Guide mới tự động vào enum của tool `get_ak_guide(topic=...)`.

Nội dung nào **không riêng EPCB** (ví dụ bổ sung ngữ nghĩa cho API kernel —
hiện mới phủ 15/54 hàm) thì nên làm trên nhánh `main` và gửi PR lên upstream,
đừng để lẫn trong nhánh `epcb`.
