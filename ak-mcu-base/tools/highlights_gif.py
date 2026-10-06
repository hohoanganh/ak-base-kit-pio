#!/usr/bin/env python3
"""
highlights_gif.py - the short GIF at the top of the README: a few seconds of
each of the best-looking screens, cut from what tests/test_demo recorded,
plus the sample clip of the Video screen.

  python tools/highlights_gif.py <folder with *.frames> port/web/clip.akv docs/demo-highlights.gif

Needs numpy and Pillow (see demo_gif.py and ak_video.py).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ak_video  # noqa: E402
import demo_gif  # noqa: E402

FRAME = demo_gif.FRAME_BYTES
# (recording, first frame, number of frames) at 20 frames per second
CUTS = [
    ("clip", 20, 100),          # the logo: one turn
    ("3d", 0, 44),
    ("maze", 0, 56),            # the walk down the first corridor
    ("tetris", 180, 60),
    ("dino", 150, 44),
    ("invaders", 60, 44),
    ("scope", 150, 40),
]
SKIP = 2                        # every 2nd frame: 10 pictures a second


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    rec, clip, out = sys.argv[1:]
    frames = []
    clip_pages = [p.tobytes() for p in ak_video.decode(open(clip, "rb").read())]
    for name, first, count in CUTS:
        if name == "clip":
            src = clip_pages
        else:
            data = open(os.path.join(rec, name + ".frames"), "rb").read()
            src = [data[i:i + FRAME] for i in range(0, len(data) - FRAME + 1, FRAME)]
        part = src[first:first + count:SKIP]
        if not part:
            raise SystemExit("recording '%s' is shorter than the cut asks for" % name)
        frames += [demo_gif.frame_image(f) for f in part]
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=1000 * SKIP // demo_gif.FPS_IN,
                   loop=0, optimize=True, disposal=1)
    print("%s: %d pictures, %.1f s, %d KB" % (out, len(frames), len(frames) * SKIP / demo_gif.FPS_IN,
                                               os.path.getsize(out) // 1024))


if __name__ == "__main__":
    main()
