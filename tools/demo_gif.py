#!/usr/bin/env python3
"""
demo_gif.py - turn the frames recorded by tests/test_demo into animated GIFs.

The frames come from the firmware's own drawing code running on the PC
(test_demo <folder> record), so the pictures show exactly what the display
of the kit shows.

  python tools/demo_gif.py <folder with *.frames> <output folder>

Needs Pillow:  pip install pillow
"""

import glob
import os
import sys

from PIL import Image

W, H, PAGES = 128, 64, 8
FRAME_BYTES = W * PAGES
SCALE = 3
BEZEL = 10
FPS_IN = 20                     # UI_FRAME_MS = 50
SKIP = 2                        # keep every 2nd frame -> 10 fps
BG, OFF, ON = (24, 26, 28), (0, 0, 0), (232, 245, 245)


def frame_image(raw):
    img = Image.new("P", (W, H), 1)
    px = img.load()
    for page in range(PAGES):
        row = raw[page * W:(page + 1) * W]
        for x in range(W):
            col = row[x]
            for bit in range(8):
                if col & (1 << bit):
                    px[x, page * 8 + bit] = 2
    img = img.resize((W * SCALE, H * SCALE), Image.NEAREST)
    out = Image.new("P", (W * SCALE + 2 * BEZEL, H * SCALE + 2 * BEZEL), 0)
    out.putpalette(BG + OFF + ON + (0, 0, 0) * 253)
    out.paste(img, (BEZEL, BEZEL))
    return out


def convert(path, out_dir):
    data = open(path, "rb").read()
    n = len(data) // FRAME_BYTES
    frames = [frame_image(data[i * FRAME_BYTES:(i + 1) * FRAME_BYTES]) for i in range(0, n, SKIP)]
    name = os.path.splitext(os.path.basename(path))[0]
    out = os.path.join(out_dir, "demo-%s.gif" % name)
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=1000 * SKIP // FPS_IN,
                   loop=0, optimize=True, disposal=1)
    print("%-10s %4d frames, %4.1f s -> %s (%d KB)" % (name, len(frames), n / FPS_IN, out, os.path.getsize(out) // 1024))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    files = sorted(glob.glob(os.path.join(src, "*.frames")))
    if not files:
        raise SystemExit("no *.frames in " + src)
    for f in files:
        convert(f, out_dir)


if __name__ == "__main__":
    main()
