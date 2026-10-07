#!/usr/bin/env python3
"""
Unit test of the frame reader in tools/ak_fw.py: response frames must be found
whatever log text the board prints around them, and the text must come out
unchanged. No board, no simulator.

  python3 tests/test_ak_fw.py --tools tools
"""

import argparse
import sys
import time


class FakeLink:
    """Hands out what the 'board' sent, in the pieces the test chose."""

    def __init__(self):
        self.chunks = []

    def write(self, data):
        pass

    def read(self):
        if self.chunks:
            return self.chunks.pop(0)
        time.sleep(0.002)
        return b""

    def close(self):
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tools", required=True)
    a = ap.parse_args()
    sys.path.insert(0, a.tools)
    import ak_fw

    fails = []

    def check(cond, what):
        print(("  ok   " if cond else "  FAIL ") + what)
        if not cond:
            fails.append(what)

    def resp(cmd, seq, payload=b""):
        return ak_fw.pack(cmd | ak_fw.RESP, seq, payload)

    def feed(pieces):
        """Run the reader over the pieces; returns (frames, text)."""
        link = FakeLink()
        link.chunks = list(pieces)
        fw = ak_fw.AkFw(link)
        end = time.time() + 2.0
        while link.chunks and time.time() < end:
            time.sleep(0.005)
        time.sleep(0.05)
        with fw.lock:
            frames, text = list(fw.frames), bytes(fw.text)
        fw.close()
        return frames, text

    f1 = resp(ak_fw.CMD_INFO, 1, b"\x00" + bytes(range(32)))
    f2 = resp(ak_fw.CMD_DATA, 2, b"\x00")

    frames, text = feed([f1])
    check(frames == [(ak_fw.CMD_INFO, 1, b"\x00" + bytes(range(32)))] and text == b"", "one frame, nothing else")

    frames, text = feed([b"[I] APP: start\r\n> ", f1, b"more text\r\n", f2])
    check([f[1] for f in frames] == [1, 2], "frames between log lines")
    check(text == b"[I] APP: start\r\n> more text\r\n", "log text comes out unchanged")

    frames, text = feed([f1[:3], f1[3:9], f1[9:], f2[:1], f2[1:]])
    check([f[1] for f in frames] == [1, 2] and text == b"", "frames arriving in pieces")

    # The case that was broken: a stray 0xA5 directly before a real frame. The
    # two bytes after it and the start of the real frame read as a length of
    # 0x0181 = 385 bytes that never arrive.
    frames, text = feed([b"log\r\n\xA5\x81", f1])
    check([f[1] for f in frames] == [1], "stray SOF right before a frame does not hide it")
    check(text == b"log\r\n\xA5\x81", "... and the stray bytes are kept as text")

    frames, text = feed([b"\xA5", f2, b"\xA5\xA5", f1])
    check([f[1] for f in frames] == [2, 1], "single stray SOF bytes around frames")

    # a false start that claims a long frame, then plain text for a while
    frames, text = feed([b"\xA5\x81\x01\xFF\x01", b"x" * 100, f2])
    check([f[1] for f in frames] == [2], "false start followed by text, then a frame")
    check(text == b"\xA5\x81\x01\xFF\x01" + b"x" * 100, "... text after the false start is not lost")

    # a frame with a damaged CRC is text, the next good one is a frame
    bad = bytearray(f1)
    bad[10] ^= 0x01
    frames, text = feed([bytes(bad), f2])
    check([f[1] for f in frames] == [2], "damaged frame dropped, next one taken")

    # a request echoed back (no response bit) is not a response
    frames, text = feed([ak_fw.pack(ak_fw.CMD_INFO, 7), f2])
    check([f[1] for f in frames] == [2], "echo of a request is not taken for a response")

    # a false start must not hold log text back for ever
    frames, text = feed([b"\xA5\x81\x01\xFF\x01"] + [b"y" * 500] * 8)
    check(len(text) >= 3000, "text after an endless false start still comes out (%d B)" % len(text))

    print("%d failed" % len(fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
