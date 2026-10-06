#!/usr/bin/env python3
"""
pong_peer.py - the PC as the second player of the Pong screen of the AK Base
Kit demo, over a USB-RS485 adapter. For trying the two-kit game with one kit.

  pong_peer.py --port COM16            # PC is the guest: the kit moves the ball
  pong_peer.py --port COM16 --host     # PC is the host: the kit is the guest

Open "Pong RS485" on the kit first. The PC plays by itself and prints the
score. The frames are described in demo/scr_pong.c (9600 8N1, like Modbus).

Needs pyserial.
"""

import argparse
import binascii
import random
import struct
import sys
import time

SOF = 0xC5
LENGTH = {ord("H"): 2, ord("S"): 7, ord("P"): 2}
TOP, FIELD_W, FIELD_H, PADDLE_H, BALL = 10, 128, 64, 12, 2
Y_MIN, Y_MAX = TOP, FIELD_H - PADDLE_H


def frame(kind, payload):
    body = bytes((ord(kind),)) + payload
    return bytes((SOF,)) + body + struct.pack("<H", binascii.crc_hqx(body, 0xFFFF))


class Link:
    def __init__(self, port, baud):
        try:
            import serial
        except ImportError:
            raise SystemExit("pyserial required: pip install pyserial")
        self.s = serial.Serial(port, baud, timeout=0.005)
        self.buf = bytearray()
        self.bad = 0

    def send(self, kind, payload):
        self.s.write(frame(kind, payload))
        self.s.flush()

    def frames(self):
        """Complete frames that arrived: list of (kind, payload)."""
        out = []
        self.buf += self.s.read(self.s.in_waiting or 1)
        while self.buf:
            if self.buf[0] != SOF:
                del self.buf[0]
                continue
            if len(self.buf) < 2:
                break
            n = LENGTH.get(self.buf[1])
            if n is None:
                del self.buf[0]
                continue
            if len(self.buf) < n + 4:
                break
            body = bytes(self.buf[1:2 + n])
            if struct.pack("<H", binascii.crc_hqx(body, 0xFFFF)) == bytes(self.buf[2 + n:4 + n]):
                out.append((chr(body[0]), body[1:]))
                del self.buf[:n + 4]
            else:
                self.bad += 1
                del self.buf[0]
        return out


def clamp(y):
    return max(Y_MIN, min(Y_MAX, y))


def guest(link, seconds):
    """The kit is the host. Say hello with the lowest number until it sends the game."""
    pad = (Y_MIN + Y_MAX) // 2
    states = replies = 0
    score = None
    last_hello = 0.0
    last_state = 0.0
    t0 = time.time()
    while time.time() - t0 < seconds:
        now = time.time()
        if now - last_state > 1.0 and now - last_hello > 0.35:
            link.send("H", struct.pack("<H", 0))
            last_hello = now
        for kind, p in link.frames():
            if kind != "S":
                continue
            seq, ball_x, ball_y, pad_host, s_host, s_guest, events = p
            last_state = now
            states += 1
            target = ball_y - PADDLE_H // 2 + 1
            pad = clamp(pad + max(-3, min(3, target - pad)))
            link.send("P", bytes((seq, pad)))
            replies += 1
            if (s_host, s_guest) != score:
                score = (s_host, s_guest)
                print("kit %d : %d pc" % score)
                sys.stdout.flush()
    print("guest: %d state frames received in %.0f s (%.1f per second), %d answered, %d damaged"
          % (states, seconds, states / seconds, replies, link.bad))
    return states


def host(link, seconds):
    """The PC moves the ball and sends the game 20 times a second."""
    bx, by, vx, vy = 63.0, 36.0, 1.6, 0.8
    pad_host = pad_guest = (Y_MIN + Y_MAX) // 2
    s_host = s_guest = 0
    seq = replies = sent = 0
    t0 = time.time()
    next_tick = t0
    while time.time() - t0 < seconds:
        for kind, p in link.frames():
            if kind == "P" and Y_MIN <= p[1] <= Y_MAX:
                pad_guest = p[1]
                replies += 1
        if time.time() < next_tick:
            continue
        next_tick += 0.05
        events = 0
        bx += vx
        by += vy
        if by < TOP or by > FIELD_H - BALL:
            vy = -vy
            by = max(TOP, min(FIELD_H - BALL, by))
            events |= 1
        pad_host = clamp(pad_host + max(-2, min(2, int(by) - PADDLE_H // 2 + 1 - pad_host)))
        if vx < 0 and bx <= 3:
            if pad_host - BALL < by < pad_host + PADDLE_H:
                vx, vy, bx, events = -vx, (by + 1 - pad_host - PADDLE_H / 2) * 0.3, 4.0, events | 1
            elif bx < 0:
                s_guest += 1
                bx, by, vx, events = 63.0, random.uniform(20, 54), 1.6, events | 2
        elif vx > 0 and bx + BALL >= FIELD_W - 4:
            if pad_guest - BALL < by < pad_guest + PADDLE_H:
                vx, vy, bx, events = -vx, (by + 1 - pad_guest - PADDLE_H / 2) * 0.3, FIELD_W - 4.0 - BALL, events | 1
            elif bx > FIELD_W - BALL:
                s_host += 1
                bx, by, vx, events = 63.0, random.uniform(20, 54), -1.6, events | 2
        if events & 2:
            print("pc %d : %d kit" % (s_host, s_guest))
            sys.stdout.flush()
        seq = (seq + 1) & 0xFF
        link.send("S", bytes((seq, int(max(0, min(127, bx))), int(by), pad_host, s_host % 100, s_guest % 100, events)))
        sent += 1
    print("host: %d state frames sent, %d paddle answers from the kit (%.0f%%), %d damaged"
          % (sent, replies, 100.0 * replies / max(1, sent), link.bad))
    return replies


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--host", action="store_true", help="the PC moves the ball, the kit is the guest")
    ap.add_argument("-t", "--seconds", type=float, default=60.0)
    a = ap.parse_args()
    link = Link(a.port, a.baud)
    ok = (host if a.host else guest)(link, a.seconds)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
