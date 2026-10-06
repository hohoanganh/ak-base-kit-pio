#!/usr/bin/env python3
"""
connectors_svg.py - draw the connectors of the AK Base Kit 3.0, pin by pin,
as an SVG for the documentation: which signal is on which pin, and which pin
of the microcontroller it comes from.

  python tools/connectors_svg.py ../docs/kit/connectors.svg

Taken from the schematic "AK MCU KIT 3.0" (sheet 6 Connectors, sheet 5 LCD,
12 May 2026). Pin 1 is the first row of every connector.
"""

import sys
from xml.sax.saxutils import escape

from pinout_svg import GROUPS, BG, CARD, LINE, TEXT, MUTED, MONO, SANS

# (reference, title, kind, note, [(pin, signal, MCU pin, group)])
CONNECTORS = [
    ("J14", "Nạp và console", "hàng chân 2,54 mm · 1×7", "Chân 1 có 3V3 khi cầu hàn SB8 đóng (mặc định)", [
        (1, "3V3", "", "power"), (2, "GND", "", "power"), (3, "SWCLK", "PA14", "debug"), (4, "SWDIO", "PA13", "debug"),
        (5, "GND", "", "power"), (6, "TX1", "PA9", "console"), (7, "RX1", "PA10", "console")]),
    ("J15", "Nạp SWD", "JST-SH 1,0 mm · 4 chân", "Chân 2 chỉ có 3V3 khi đóng SB9 (mặc định hở)", [
        (1, "GND", "", "power"), (2, "3V3", "", "power"), (3, "SWCLK", "PA14", "debug"), (4, "SWDIO", "PA13", "debug")]),
    ("J12", "Console UART1", "JST-SH 1,0 mm · 4 chân", "Chân 2 có 5V khi SB7 đóng (mặc định)", [
        (1, "GND", "", "power"), (2, "5V", "", "power"), (3, "TX1", "PA9", "console"), (4, "RX1", "PA10", "console")]),
    ("J10", "RS485", "JST-SH 1,0 mm · 4 chân", "Chân 2 chỉ có 5V khi đóng SB6 (mặc định hở)", [
        (1, "GND", "", "power"), (2, "V_RS485", "", "power"), (3, "B", "", "rs485"), (4, "A", "", "rs485")]),
    ("J4", "UART3", "JST-SH 1,0 mm · 4 chân (Qwiic)", "Nguồn chân 2: 3V3 mặc định, đổi sang 5V bằng SB4", [
        (1, "GND", "", "power"), (2, "V_UART3", "", "power"), (3, "TX3", "PB10", "header"), (4, "RX3", "PB11", "header")]),
    ("J7", "I2C1", "JST-SH 1,0 mm · 4 chân (Qwiic)", "Nguồn chân 2: 3V3 mặc định, đổi sang 5V bằng SB5", [
        (1, "GND", "", "power"), (2, "V_I2C", "", "power"), (3, "SDA", "PB7", "i2c"), (4, "SCL", "PB6", "i2c")]),
    ("J13", "Chân vi điều khiển", "hàng chân 2,54 mm · 2×6", "PC14, PC15 tới đây qua cầu hàn SB13, SB14 (mặc định đóng)", [
        (1, "GND", "", "power"), (2, "3V3", "", "power"), (3, "PA0", "PA0", "header"), (4, "PC14", "PC14", "header"),
        (5, "PA11", "PA11", "header"), (6, "PC15", "PC15", "header"), (7, "PA12", "PA12", "header"), (8, "PB5", "PB5", "header"),
        (9, "PA15", "PA15", "oled"), (10, "PB9", "PB9", "header"), (11, "GND", "", "power"), (12, "PB15", "PB15", "header")]),
    ("J6", "SPI mở rộng", "hàng chân 2,54 mm · 2×5", "Xếp chân như mô-đun nRF24L01+; cũng dùng cho W5500", [
        (1, "GND", "", "power"), (2, "3V3", "", "power"), (3, "CE", "PA8", "header"), (4, "CSN", "PA4", "header"),
        (5, "SCK", "PA5", "flash"), (6, "MOSI", "PA7", "flash"), (7, "MISO", "PA6", "flash"), (8, "IRQ", "PB1", "header"),
        (9, "GND", "", "power"), (10, "NRST", "NRST", "power")]),
    ("J9", "I2C và UART mở rộng", "hàng chân 2,54 mm · 2×5", "", [
        (1, "GND", "", "power"), (2, "3V3", "", "power"), (3, "SCL", "PB6", "i2c"), (4, "GND", "", "power"),
        (5, "SDA", "PB7", "i2c"), (6, "GND", "", "power"), (7, "TX3", "PB10", "header"), (8, "RX3", "PB11", "header"),
        (9, "GND", "", "power"), (10, "5V", "", "power")]),
    ("J3", "Màn hình OLED", "hàng chân 2,54 mm · 1×7", "CS, DC không nối vi điều khiển; màn hình chạy I2C (0x3C)", [
        (1, "GND", "", "power"), (2, "3V3", "", "power"), (3, "SCL", "PB13", "oled"), (4, "SDA", "PB12", "oled"),
        (5, "RES", "PA15", "oled"), (6, "DC", "", "power"), (7, "CS", "", "power")]),
]

NOTES = [
    "USB Type-C (J1): cấp nguồn 5V và nối console UART1 với máy tính qua CH340E. Theo schematic, cầu hàn SB1, SB2 mặc định nối "
    "TX1, RX1 vào CH340E; đổi sang vị trí kia thì console ra J12 và J14.",
    "Ba cổng Grove 2,0 mm (J5 UART3, J8 I2C1, J11 RS485) mang cùng tín hiệu với J4, J7, J10; schematic bản PVT ghi chúng không lắp.",
    "Flash SPI trên bo dùng chung SCK, MOSI, MISO với J6: mô-đun cắm vào J6 phải có chân chọn chip riêng (CSN ở PA4).",
]

W = 1280
COLS = 3
CARD_W = 384
GAP = 24
ROW_H = 26
HEAD = 78


def card_height(c):
    return HEAD + len(c[4]) * ROW_H + (34 if c[3] else 14)


def wrap(text, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        if len(cur) + len(w) + 1 > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    if cur:
        lines.append(cur)
    return lines


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "connectors.svg"
    # place the cards column by column, always into the shortest column
    tops = [128] * COLS
    placed = []
    for c in CONNECTORS:
        col = tops.index(min(tops))
        placed.append((c, 48 + col * (CARD_W + GAP), tops[col]))
        tops[col] += card_height(c) + GAP
    notes_y = max(tops) + 14
    note_lines = [wrap(n, 132) for n in NOTES]
    H = notes_y + sum(len(n) * 24 + 12 for n in note_lines) + 34

    s = []
    a = s.append
    a('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" role="img" aria-labelledby="t d">'
      % (W, H, W, H))
    a('<title id="t">Các cổng nối của AK Base Kit 3.0</title>')
    a('<desc id="d">Mười cổng nối của bo, mỗi cổng liệt kê từng chân: tín hiệu và chân vi điều khiển tương ứng.</desc>')
    a('<rect width="%d" height="%d" fill="%s"/>' % (W, H, BG))
    a('<text x="48" y="64" fill="%s" font-family="%s" font-size="30" font-weight="700">Các cổng nối của AK Base Kit</text>'
      % (TEXT, SANS))
    a('<text x="48" y="96" fill="%s" font-family="%s" font-size="17">Chân nào mang tín hiệu gì, nối về chân nào của vi điều khiển · '
      'theo schematic AK MCU KIT 3.0</text>' % (MUTED, SANS))

    for c, x, y in placed:
        ref, title, kind, note, pins = c
        h = card_height(c)
        a('<rect x="%d" y="%d" width="%d" height="%d" rx="10" fill="%s" stroke="%s"/>' % (x, y, CARD_W, h, CARD, LINE))
        a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="21" font-weight="700">%s</text>'
          % (x + 18, y + 32, "#a8ff3e", MONO, ref))
        a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="19" font-weight="600">%s</text>'
          % (x + 18 + 14 * len(ref) + 10, y + 32, TEXT, SANS, escape(title)))
        a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="14">%s</text>' % (x + 18, y + 56, MUTED, SANS, escape(kind)))
        for i, (num, sig, mcu, group) in enumerate(pins):
            py = y + HEAD + i * ROW_H
            colour = GROUPS[group][0]
            a('<rect x="%d" y="%d" width="30" height="20" rx="3" fill="%s"/>' % (x + 18, py - 2, colour))
            a('<text x="%d" y="%d" text-anchor="middle" fill="%s" font-family="%s" font-size="14" font-weight="700">%d</text>'
              % (x + 33, py + 13, BG, MONO, num))
            a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="16" font-weight="700">%s</text>'
              % (x + 62, py + 13, TEXT, MONO, escape(sig)))
            if mcu:
                a('<text x="%d" y="%d" text-anchor="end" fill="%s" font-family="%s" font-size="16">%s</text>'
                  % (x + CARD_W - 18, py + 13, colour, MONO, escape(mcu)))
        if note:
            a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="13">%s</text>'
              % (x + 18, y + h - 14, MUTED, SANS, escape(note)))

    a('<line x1="48" y1="%d" x2="%d" y2="%d" stroke="%s"/>' % (notes_y - 14, W - 48, notes_y - 14, LINE))
    y = notes_y + 14
    for lines in note_lines:
        for k, line in enumerate(lines):
            a('<text x="%d" y="%d" fill="%s" font-family="%s" font-size="16">%s</text>'
              % (48, y, TEXT if k == 0 else TEXT, SANS, escape(line)))
            y += 24
        y += 12
    a('</svg>')
    open(out, "w", encoding="utf-8").write("\n".join(s) + "\n")
    print(out, W, H)


if __name__ == "__main__":
    main()
