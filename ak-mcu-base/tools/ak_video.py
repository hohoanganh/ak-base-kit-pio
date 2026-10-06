#!/usr/bin/env python3
"""
ak_video.py - make 1-bit video clips for the demo of the AK Base Kit and load
them into the SPI flash of the kit (screen "Video").

  ak_video.py convert clip.mp4 -o clip.akv            # video file (needs opencv-python)
  ak_video.py convert anim.gif -o clip.akv --fps 10   # GIF, or a folder of pictures
  ak_video.py convert tour.frames -o clip.akv         # frames recorded by tests/test_demo
  ak_video.py info clip.akv
  ak_video.py preview clip.akv out.gif                # what the kit will show
  ak_video.py upload --port COM5 clip.akv             # kit must run the demo firmware

The picture is scaled to 128x64 and made black and white (threshold, or
--dither). A frame stores only the display pages that changed, each one
run-length packed, either as it is or as the difference to the frame before,
whichever is shorter. The format is described in demo/video.h.

Needs numpy and Pillow; video files also need opencv-python; upload needs pyserial.
"""

import argparse
import glob
import os
import struct
import sys
import time

import numpy as np
from PIL import Image, ImageSequence

W, H, PAGES = 128, 64, 8
HEADER = 16
MAGIC = b"AKV1"
CMD_STORE_INFO, CMD_STORE_ERASE, CMD_STORE_WRITE = 0x40, 0x41, 0x42
CHUNK = 128
BIT_WEIGHTS = (1 << np.arange(8)).reshape(1, 8, 1)


# ------------------------------------------------------------------------------
# pictures -> display pages
# ------------------------------------------------------------------------------
def to_pages(img, threshold, dither, fill, invert):
    """PIL image -> 8 x 128 bytes as the display takes them (bit 0 = top pixel)."""
    img = img.convert("L")
    scale = (max if fill else min)(W / img.width, H / img.height)
    size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
    img = img.resize(size, Image.LANCZOS)
    canvas = Image.new("L", (W, H), 0)
    canvas.paste(img, ((W - size[0]) // 2, (H - size[1]) // 2))
    if dither:
        bits = np.array(canvas.convert("1"), dtype=bool)
    else:
        bits = np.array(canvas) >= threshold
    if invert:
        bits = ~bits
    return (bits.reshape(PAGES, 8, W) * BIT_WEIGHTS).sum(axis=1).astype(np.uint8)


def read_source(path, fps, start, duration):
    """Yields (PIL image or page array) at the wanted frame rate. Returns (iterator, fps)."""
    ext = os.path.splitext(path)[1].lower()
    if ext == ".frames":
        data = open(path, "rb").read()
        frames = [np.frombuffer(data[i:i + W * PAGES], dtype=np.uint8).reshape(PAGES, W)
                  for i in range(0, len(data) - W * PAGES + 1, W * PAGES)]
        src_fps = 20.0
        step = src_fps / fps
        picked = [frames[int(i * step)] for i in range(int(len(frames) / step))]
        return picked, fps
    if os.path.isdir(path):
        files = sorted(glob.glob(os.path.join(path, "*.png")) + glob.glob(os.path.join(path, "*.jpg"))
                       + glob.glob(os.path.join(path, "*.bmp")))
        return [Image.open(f) for f in files], fps
    if ext == ".gif":
        gif = Image.open(path)
        shots, t = [], 0.0
        for frame in ImageSequence.Iterator(gif):
            shots.append((t, frame.convert("L")))
            t += frame.info.get("duration", 100) / 1000.0
        out, k = [], 0
        for i in range(int(t * fps)):
            while k + 1 < len(shots) and shots[k + 1][0] <= i / fps:
                k += 1
            out.append(shots[k][1])
        return out, fps

    try:
        import cv2
    except ImportError:
        raise SystemExit("video files need opencv:  pip install opencv-python")
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        raise SystemExit("cannot open " + path)
    src_fps = cap.get(cv2.CAP_PROP_FPS) or 30.0

    def frames():
        want, n = 0, 0
        while True:
            ok, bgr = cap.read()
            if not ok:
                return
            t = n / src_fps
            n += 1
            if t < start or (duration and t >= start + duration):
                if duration and t >= start + duration:
                    return
                continue
            if (t - start) * fps >= want:
                want += 1
                yield Image.fromarray(cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY))
    return frames(), fps


# ------------------------------------------------------------------------------
# codec
# ------------------------------------------------------------------------------
def packbits(data):
    out = bytearray()
    n, i = len(data), 0
    while i < n:
        run = 1
        while i + run < n and data[i + run] == data[i] and run < 129:
            run += 1
        if run >= 2:
            out += bytes((run + 126, data[i]))
            i += run
            continue
        lit = 1
        while i + lit < n and lit < 128 and not (i + lit + 1 < n and data[i + lit] == data[i + lit + 1]):
            lit += 1
        out.append(lit - 1)
        out += data[i:i + lit]
        i += lit
    return bytes(out)


def encode(frames, fps):
    """frames: list of uint8 arrays (PAGES, W). Returns the .akv file as bytes."""
    body = bytearray()
    prev = None
    for cur in frames:
        changed = xor = 0
        packed = bytearray()
        for page in range(PAGES):
            row = cur[page].tobytes()
            if prev is None:
                changed |= 1 << page
                packed += packbits(row)
            elif row != prev[page].tobytes():
                changed |= 1 << page
                plain = packbits(row)
                diff = packbits((cur[page] ^ prev[page]).tobytes())
                if len(diff) < len(plain):
                    xor |= 1 << page
                    packed += diff
                else:
                    packed += plain
        body += bytes((changed, xor)) + packed
        prev = cur
    if not frames:
        raise SystemExit("no frames")
    if len(frames) > 65535:
        raise SystemExit("too many frames (%d), 65535 at most: lower --fps or --duration" % len(frames))
    return MAGIC + struct.pack("<BBBBHHI", W, H, fps, 0, len(frames), 0, len(body)) + bytes(body)


def parse_header(data):
    if len(data) < HEADER or data[:4] != MAGIC:
        raise SystemExit("not an .akv file")
    w, h, fps, _, n, _, size = struct.unpack("<BBBBHHI", data[4:HEADER])
    if w != W or h != H or fps == 0 or HEADER + size > len(data):
        raise SystemExit("damaged .akv file")
    return fps, n, size


def decode(data):
    """Generator of page arrays; the same steps as demo/video.c."""
    fps, n, size = parse_header(data)
    fb = np.zeros((PAGES, W), dtype=np.uint8)
    p = HEADER
    for _ in range(n):
        changed, xor = data[p], data[p + 1]
        p += 2
        for page in range(PAGES):
            if not (changed >> page) & 1:
                continue
            row = bytearray()
            while len(row) < W:
                c = data[p]
                p += 1
                if c < 128:
                    row += data[p:p + c + 1]
                    p += c + 1
                else:
                    row += bytes((data[p],)) * (c - 126)
                    p += 1
            if len(row) != W:
                raise SystemExit("damaged .akv file")
            row = np.frombuffer(bytes(row), dtype=np.uint8)
            fb[page] = fb[page] ^ row if (xor >> page) & 1 else row
        yield fb.copy()
    if p != HEADER + size:
        raise SystemExit("damaged .akv file: length does not match")


def pages_to_image(pages, scale=3):
    bits = ((pages.reshape(PAGES, 1, W) >> np.arange(8).reshape(1, 8, 1)) & 1).reshape(H, W)
    return Image.fromarray((bits * 255).astype(np.uint8)).resize((W * scale, H * scale), Image.NEAREST)


# ------------------------------------------------------------------------------
# commands
# ------------------------------------------------------------------------------
def cmd_convert(a):
    source, fps = read_source(a.input, a.fps, a.start, a.duration)
    frames = []
    for item in source:
        frames.append(item if isinstance(item, np.ndarray) else to_pages(item, a.threshold, a.dither, a.fill, a.invert))
    data = encode(frames, fps)
    if any(not np.array_equal(x, y) for x, y in zip(frames, decode(data))):
        raise SystemExit("internal error: the clip does not decode to its source")
    if a.max_size and len(data) > a.max_size:
        raise SystemExit("clip is %d bytes, more than --max-size %d: lower --fps, shorten it (--duration) "
                         "or drop --dither" % (len(data), a.max_size))
    open(a.output, "wb").write(data)
    print("%s: %d frames at %d fps = %.1f s, %d bytes (%d per frame, %.0f%% of raw)"
          % (a.output, len(frames), fps, len(frames) / fps, len(data), len(data) // len(frames),
             100.0 * len(data) / (len(frames) * W * PAGES)))


def cmd_info(a):
    data = open(a.file, "rb").read()
    fps, n, size = parse_header(data)
    sum(1 for _ in decode(data))
    print("%s: %d frames at %d fps = %.1f s, %d bytes, decodes cleanly" % (a.file, n, fps, n / fps, HEADER + size))


def cmd_preview(a):
    data = open(a.file, "rb").read()
    fps, _, _ = parse_header(data)
    imgs = [pages_to_image(p).convert("P") for p in decode(data)]
    imgs[0].save(a.output, save_all=True, append_images=imgs[1:], duration=1000 // fps, loop=0, optimize=True)
    print("%s: %d frames" % (a.output, len(imgs)))


def cmd_upload(a):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import ak_fw

    data = open(a.file, "rb").read()
    fps, n, size = parse_header(data)
    data = data[:HEADER + size]
    fw = ak_fw.AkFw(ak_fw.SerialLink(a.port, a.baud))
    try:
        r = fw.request(CMD_STORE_INFO)
        if not r or r[0] != 0 or len(r) < 9:
            raise SystemExit("the firmware on the kit has no media store (flash the demo: pio run -e demo)")
        store, sector = struct.unpack("<II", r[1:9])
        if store == 0:
            raise SystemExit("the kit did not find its SPI flash")
        if len(data) > store:
            raise SystemExit("clip is %d bytes, the store holds %d" % (len(data), store))
        print("store: %d bytes, clip: %d bytes (%d frames, %.1f s)" % (store, len(data), n, n / fps))
        t0 = time.time()
        for off in range(0, len(data), CHUNK):
            if off % sector == 0:
                r = fw.request(CMD_STORE_ERASE, struct.pack("<I", off), timeout=3.0)
                if not r or r[0] != 0:
                    raise SystemExit("\nerase failed at 0x%X" % off)
            r = fw.request(CMD_STORE_WRITE, struct.pack("<I", off) + data[off:off + CHUNK], timeout=2.0)
            if not r or r[0] != 0:
                raise SystemExit("\nwrite failed at 0x%X" % off)
            sys.stdout.write("\r%3d%%" % (100 * min(off + CHUNK, len(data)) // len(data)))
            sys.stdout.flush()
        print("\rdone in %.0f s. Open \"Video\" in the menu of the kit." % (time.time() - t0))
    finally:
        fw.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("convert", help="video / GIF / folder of pictures / .frames -> .akv")
    p.add_argument("input")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--fps", type=int, default=15, help="frames per second, 1..20 (default 15)")
    p.add_argument("--threshold", type=int, default=128, help="grey level that becomes white (default 128)")
    p.add_argument("--dither", action="store_true", help="dither instead of a threshold (bigger file)")
    p.add_argument("--fill", action="store_true", help="fill the screen and crop, instead of fitting with bars")
    p.add_argument("--invert", action="store_true")
    p.add_argument("--start", type=float, default=0.0, help="video files: skip this many seconds")
    p.add_argument("--duration", type=float, default=0.0, help="video files: stop after this many seconds")
    p.add_argument("--max-size", type=int, default=512 * 1024, help="fail above this size (default: the 512 KB store)")
    p.set_defaults(func=cmd_convert)

    p = sub.add_parser("info", help="check an .akv file")
    p.add_argument("file")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("preview", help=".akv -> GIF")
    p.add_argument("file")
    p.add_argument("output")
    p.set_defaults(func=cmd_preview)

    p = sub.add_parser("upload", help="write an .akv file into the SPI flash of the kit")
    p.add_argument("file")
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=115200)
    p.set_defaults(func=cmd_upload)

    a = ap.parse_args()
    if a.cmd == "convert" and not 1 <= a.fps <= 20:
        raise SystemExit("--fps must be 1..20 (the display is refreshed 20 times a second)")
    a.func(a)


if __name__ == "__main__":
    main()
