#!/usr/bin/env python3
"""
spin_clip.py - turn a logo (any picture on a plain background) into a clip of
it spinning in 3D, for the "Video" screen of the AK Base Kit demo.

  spin_clip.py logo.png -o clip.akv
  spin_clip.py logo.png -o clip.akv --preview clip.gif --fps 20 --turn 5

The picture becomes a slab with some thickness that turns around its vertical
axis, leaning a little towards the viewer. The face is white, the sides are
dithered grey, so the turn reads as depth on a 1-bit display. Then load it:

  ak_video.py upload --port COMx clip.akv

Needs numpy, Pillow and opencv-python.
"""

import argparse
import math
import os
import sys

import cv2
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ak_video  # noqa: E402

W, H, PAGES = ak_video.W, ak_video.H, ak_video.PAGES
SS = 8                                  # render at 8x and scale down: smooth edges before the threshold
BIT_WEIGHTS = (1 << np.arange(8)).reshape(1, 8, 1)


def load_mask(path, size):
    """The picture as a square white-on-black mask: whatever differs from the corner colour."""
    img = Image.open(path).convert("RGBA")
    flat = Image.new("RGBA", img.size, (255, 255, 255, 255))
    flat.alpha_composite(img)
    rgb = np.asarray(flat.convert("RGB")).astype(np.int32)
    diff = np.abs(rgb - rgb[0, 0]).sum(axis=2)
    mask = (diff > 120).astype(np.uint8) * 255
    ys, xs = np.nonzero(mask)
    mask = mask[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    side = max(mask.shape)
    square = np.zeros((side, side), np.uint8)
    oy, ox = (side - mask.shape[0]) // 2, (side - mask.shape[1]) // 2
    square[oy:oy + mask.shape[0], ox:ox + mask.shape[1]] = mask
    big = cv2.resize(square, (size, size), interpolation=cv2.INTER_CUBIC)
    big = cv2.GaussianBlur(big, (0, 0), size / 200.0)      # a small source picture: round off its pixel steps
    return (big > 127).astype(np.uint8) * 255


def project(points, yaw, tilt, focal, dist, cx, cy):
    """Model points (x, y, z) -> screen (x, y) and depth after the turn."""
    x, y, z = points[:, 0], points[:, 1], points[:, 2]
    x1 = x * math.cos(yaw) + z * math.sin(yaw)
    z1 = z * math.cos(yaw) - x * math.sin(yaw)
    y1 = y * math.cos(tilt) - z1 * math.sin(tilt)
    z2 = y * math.sin(tilt) + z1 * math.cos(tilt)
    scale = focal / (z2 + dist)
    return np.stack([cx + x1 * scale, cy + y1 * scale], axis=1).astype(np.float32), z2


def render(mask, yaw, tilt, half, thick, layers):
    """One frame at SS times the display size: (face, body) as boolean pictures."""
    cw, ch = W * SS, H * SS
    size = mask.shape[0]
    src = np.float32([[0, 0], [size, 0], [size, size], [0, size]])
    focal, dist = 6.0 * half, 6.0 * half               # a distant viewer: little perspective
    if math.cos(yaw) < 0:
        mask = mask[:, ::-1]                            # the back shows the picture too, readable, not mirrored
    body = np.zeros((ch, cw), np.uint8)
    face, face_depth = None, None
    for k in range(layers):
        z = -thick + 2.0 * thick * k / (layers - 1)
        corners = np.float32([[-half, -half, z], [half, -half, z], [half, half, z], [-half, half, z]])
        dst, depth = project(corners, yaw, tilt, focal, dist, cw / 2.0, ch / 2.0)
        layer = cv2.warpPerspective(mask, cv2.getPerspectiveTransform(src, dst), (cw, ch), flags=cv2.INTER_LINEAR)
        body = np.maximum(body, layer)
        if face_depth is None or depth.mean() < face_depth:      # the layer nearest to the viewer
            face, face_depth = layer, depth.mean()
    return face > 127, body > 127


def to_pages(face, body):
    """Scale down, face white, the rest of the slab a 50% checker."""
    f = cv2.resize(face.astype(np.uint8) * 255, (W, H), interpolation=cv2.INTER_AREA) > 110
    b = cv2.resize(body.astype(np.uint8) * 255, (W, H), interpolation=cv2.INTER_AREA) > 110
    yy, xx = np.mgrid[0:H, 0:W]
    bits = f | (b & (((xx + yy) & 1) == 0))
    return (bits.reshape(PAGES, 8, W) * BIT_WEIGHTS).sum(axis=1).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--fps", type=int, default=20, help="frames per second, 1..20 (default 20)")
    ap.add_argument("--turn", type=float, default=5.0, help="seconds for one full turn (default 5)")
    ap.add_argument("--hold", type=float, default=1.0, help="seconds the picture rests facing the viewer (default 1)")
    ap.add_argument("--tilt", type=float, default=9.0, help="lean towards the viewer, degrees (default 9)")
    ap.add_argument("--thickness", type=float, default=0.07, help="thickness of the slab, part of its height (default 0.07)")
    ap.add_argument("--size", type=float, default=0.86, help="height of the picture, part of the screen height (default 0.86)")
    ap.add_argument("--preview", help="also write a GIF of the clip")
    a = ap.parse_args()
    if not 1 <= a.fps <= 20:
        raise SystemExit("--fps must be 1..20")

    half = H * SS * a.size / 2.0
    mask = load_mask(a.image, 512)
    frames = []
    for _ in range(int(a.hold * a.fps)):
        frames.append(to_pages(*render(mask, 0.0, math.radians(a.tilt), half, a.thickness * 2 * half, 9)))
    n = int(a.turn * a.fps)
    for i in range(n):
        # ease in and out of the rest position, steady in between
        t = (i + 1) / n
        yaw = 2 * math.pi * (t - math.sin(2 * math.pi * t) / (2 * math.pi) * 0.6)
        frames.append(to_pages(*render(mask, yaw, math.radians(a.tilt), half, a.thickness * 2 * half, 9)))

    data = ak_video.encode(frames, a.fps)
    if any(not np.array_equal(x, y) for x, y in zip(frames, ak_video.decode(data))):
        raise SystemExit("internal error: the clip does not decode to its source")
    open(a.output, "wb").write(data)
    print("%s: %d frames at %d fps = %.1f s, %d bytes (%d per frame)"
          % (a.output, len(frames), a.fps, len(frames) / a.fps, len(data), len(data) // len(frames)))
    if a.preview:
        imgs = [ak_video.pages_to_image(p).convert("P") for p in frames]
        imgs[0].save(a.preview, save_all=True, append_images=imgs[1:], duration=1000 // a.fps, loop=0)
        print(a.preview)


if __name__ == "__main__":
    main()
