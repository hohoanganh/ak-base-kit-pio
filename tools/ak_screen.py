#!/usr/bin/env python3
"""
ak_screen.py - see the display of the AK Base Kit demo on the PC, over the
console UART, and press its buttons from the keyboard.

  ak_screen.py --port COM5 shot screen.png          # one screenshot
  ak_screen.py --port COM5 record clip.gif -t 20    # 20 seconds as a GIF
  ak_screen.py --port COM5 live                     # a window that follows the kit

In the live window:  1 2 3 = buttons B1 B2 B3,  b = hold B3 (back to the menu),
a = games play themselves on/off,  s = save a screenshot,  q = quit.

The kit sends the pages of its frame buffer that changed as lines of text
("@P<page> <hex> <crc>", see demo/ui.h); nothing else on the console is
disturbed, so the shell keeps working. The picture is what the firmware drew,
not a photo of the OLED. While the mirror runs, heavy screens get slower:
the text has to fit through 115200 baud.

Needs pyserial; shot / record / s also need Pillow. live uses tkinter.
"""

import argparse
import binascii
import sys
import time

W, H, PAGES = 128, 64, 8


class Mirror:
    def __init__(self, port, baud):
        try:
            import serial
        except ImportError:
            raise SystemExit("pyserial required: pip install pyserial")
        self.s = serial.Serial(port, baud, timeout=0.02)
        self.buf = b""
        self.pages = [bytes(W)] * PAGES
        self.seen = 0               # bit n: page n received at least once
        self.bad = 0                # lines that failed their checksum
        self.text = []              # other console lines

    def close(self):
        self.s.close()

    def cmd(self, line):
        self.s.write((line + "\r").encode())

    def poll(self):
        """Reads what arrived. Returns True when a batch of pages ended (redraw)."""
        redraw = False
        self.buf += self.s.read(self.s.in_waiting or 1)
        while b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            line = line.strip()
            if line.startswith(b"> "):
                line = line[2:]
            if line == b"@E":
                redraw = True
            elif line.startswith(b"@P"):
                if not self._page(line):
                    self.bad += 1
            elif line:
                self.text.append(line.decode("ascii", "replace"))
        return redraw

    def _page(self, line):
        try:
            head, packed, crc = line.split(b" ")
            page = int(head[2:])
            packed = bytes.fromhex(packed.decode())
            crc = int(crc, 16)
        except ValueError:
            return False
        out, i = bytearray(), 0
        while i < len(packed) and len(out) < W:
            c = packed[i]
            i += 1
            if c < 128:
                out += packed[i:i + c + 1]
                i += c + 1
            elif i < len(packed):
                out += bytes((packed[i],)) * (c - 126)
                i += 1
        if not 0 <= page < PAGES or len(out) != W or binascii.crc_hqx(bytes(out), 0xFFFF) != crc:
            return False
        self.pages[page] = bytes(out)
        self.seen |= 1 << page
        return True

    def stream(self, on):
        """Turns the stream on or off (the shell command is a toggle, so read its answer)."""
        for _ in range(3):
            self.text.clear()
            self.cmd("ui stream")
            end = time.time() + 0.6
            while time.time() < end:
                self.poll()
                state = [t for t in self.text if t.startswith("stream ")]
                if state:
                    if state[-1].endswith("on" if on else "off"):
                        return
                    break
        if on:
            raise SystemExit("no answer to 'ui stream': is the demo firmware (v1.3.2 or newer) running?")

    def image(self, scale=4):
        from PIL import Image

        img = Image.new("L", (W, H), 0)
        px = img.load()
        for page in range(PAGES):
            row = self.pages[page]
            for x in range(W):
                c = row[x]
                for bit in range(8):
                    if c & (1 << bit):
                        px[x, page * 8 + bit] = 255
        return img.resize((W * scale, H * scale), 0)


def cmd_shot(a):
    m = Mirror(a.port, a.baud)
    try:
        for _ in range(3):
            m.seen = 0
            m.cmd("ui dump")
            end = time.time() + 2.0
            while time.time() < end and m.seen != 0xFF:
                m.poll()
            if m.seen == 0xFF:
                break
        else:
            raise SystemExit("no screen received: is the demo firmware (v1.3.2 or newer) running?")
        m.image(a.scale).save(a.output)
        print("%s saved" % a.output)
    finally:
        m.close()


def cmd_record(a):
    m = Mirror(a.port, a.baud)
    shots = []
    try:
        m.stream(True)
        t0 = time.time()
        while time.time() - t0 < a.seconds:
            if m.poll() and m.seen == 0xFF:
                shots.append((time.time() - t0, m.image(a.scale).convert("P")))
        m.stream(False)
    finally:
        m.close()
    if len(shots) < 2:
        raise SystemExit("nothing changed on the screen in %.0f s" % a.seconds)
    # each picture stays until the next one arrived
    times = [max(20, int((shots[i + 1][0] - shots[i][0]) * 1000)) for i in range(len(shots) - 1)] + [200]
    shots[0][1].save(a.output, save_all=True, append_images=[s[1] for s in shots[1:]], duration=times, loop=0)
    print("%s: %d pictures in %.1f s (%.1f per second), %d damaged line(s)"
          % (a.output, len(shots), shots[-1][0], len(shots) / shots[-1][0], m.bad))


def cmd_live(a):
    import tkinter as tk

    m = Mirror(a.port, a.baud)
    m.stream(True)
    root = tk.Tk()
    root.title("AK Base Kit - %s" % a.port)
    root.configure(bg="#181a1c")
    img = tk.PhotoImage(width=W, height=H)
    shown = img.zoom(a.scale)
    label = tk.Label(root, image=shown, bd=10, bg="#181a1c")
    label.pack()
    tk.Label(root, text="1 2 3: buttons    b: back    a: autoplay    s: screenshot    q: quit",
             fg="#c8d2d2", bg="#181a1c").pack(pady=(0, 8))
    state = {"shown": shown, "n": 0}

    def redraw():
        rows = []
        for y in range(H):
            page, bit = m.pages[y >> 3], 1 << (y & 7)
            rows.append("{" + " ".join("#e8f5f5" if page[x] & bit else "#000000" for x in range(W)) + "}")
        img.put(" ".join(rows))
        state["shown"] = img.zoom(a.scale)
        label.configure(image=state["shown"])

    def tick():
        if m.poll():
            redraw()
        root.after(15, tick)

    def key(e):
        c = e.char.lower()
        if c in ("1", "2", "3"):
            m.cmd("ui " + c)
        elif c == "b":
            m.cmd("ui back")
        elif c == "a":
            m.cmd("ui auto")
        elif c == "s":
            state["n"] += 1
            name = "kit-screen-%d.png" % state["n"]
            m.image(4).save(name)
            root.title("saved " + name)
        elif c == "q":
            root.destroy()

    root.bind("<Key>", key)
    root.after(15, tick)
    try:
        root.mainloop()
    finally:
        m.stream(False)
        m.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--scale", type=int, default=4, help="pixels on the PC per pixel of the kit (default 4)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("shot", help="one screenshot")
    p.add_argument("output")
    p.set_defaults(func=cmd_shot)
    p = sub.add_parser("record", help="a GIF of what the kit shows")
    p.add_argument("output")
    p.add_argument("-t", "--seconds", type=float, default=10.0)
    p.set_defaults(func=cmd_record)
    p = sub.add_parser("live", help="a window that follows the kit; keys press its buttons")
    p.set_defaults(func=cmd_live)
    a = ap.parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
