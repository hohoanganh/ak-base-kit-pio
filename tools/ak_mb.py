#!/usr/bin/env python3
"""
ak_mb.py - talk to an ak-mcu-base board over Modbus RTU / RS485.

  ak_mb.py --port COM16 info                      # version, uptime, crash count
  ak_mb.py --port COM16 read 16 4                 # holding registers 16..19
  ak_mb.py --port COM16 write 16 123 456          # write holding registers
  ak_mb.py --port COM16 flash app.img             # firmware update over RS485
  ak_mb.py --port COM16 --unit 3 --baud 19200 info

flash sends the .img made by tools/mkimage.py (or the build) through the
register block 0xF000 (services/modbus/mb_ota.h): header, BEGIN, 128-byte
chunks, COMMIT. The board verifies the whole image before it answers COMMIT,
then resets and the bootloader installs it. A broken transfer leaves the
running firmware untouched: just run flash again.

Needs pyserial:  pip install pyserial
"""

import argparse
import struct
import sys
import time

REG_CMD, REG_STATUS, REG_LEN_HI, REG_RECV_HI, REG_DETAIL, REG_CHUNK = 0xF000, 0xF001, 0xF002, 0xF007, 0xF009, 0xF010
CMD_BEGIN, CMD_COMMIT, CMD_ABORT = 1, 2, 3
PSK_IMG = 0x57464B41            # image type of ak-mcu-base ("AKFW")
IMG_MAGIC = 0x57464B41
CHUNK = 128

STATUS = {0: "idle", 1: "receiving", 2: "committed", 0x8001: "header error", 0x8002: "offset error",
          0x8003: "image crc error", 0x8004: "size error"}
# keep in sync with services/fw/fw_types.h
FW_ERRORS = {0: "ok", 1: "bad arg", 2: "bad state", 3: "too big", 4: "flash error", 5: "no image",
             6: "header crc", 7: "wrong board", 8: "wrong load addr", 9: "image crc", 10: "bad offset",
             11: "unknown cmd", 12: "bad size", 13: "bad vector table"}
EXCEPTIONS = {1: "illegal function", 2: "illegal data address", 3: "illegal data value", 4: "device failure"}


class MbError(Exception):
    pass


class MbException(MbError):
    def __init__(self, code):
        super().__init__("modbus exception %d: %s" % (code, EXCEPTIONS.get(code, "?")))
        self.code = code


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


class Master:
    def __init__(self, port, baud, unit):
        try:
            import serial
        except ImportError:
            raise SystemExit("pyserial required: pip install pyserial")
        self.s = serial.Serial(port, baud, timeout=0)
        self.unit = unit
        self.gap = max(0.004, 38.5 / baud)      # 3.5 characters of silence between frames

    def close(self):
        self.s.close()

    def _xfer(self, pdu, resp_len, timeout):
        frame = bytes([self.unit]) + pdu
        frame += struct.pack("<H", crc16(frame))
        time.sleep(self.gap)
        self.s.reset_input_buffer()
        self.s.write(frame)
        self.s.flush()
        buf = b""
        end = time.time() + timeout
        while time.time() < end:
            d = self.s.read(256)
            if d:
                buf += d
                # an exception answer is 5 bytes, a normal one resp_len
                need = 5 if len(buf) >= 2 and buf[1] & 0x80 else resp_len
                if len(buf) >= need:
                    buf = buf[:need]
                    break
            else:
                time.sleep(0.001)
        if len(buf) < 5:
            raise MbError("no answer (got %d byte(s))" % len(buf))
        if crc16(buf[:-2]) != struct.unpack("<H", buf[-2:])[0]:
            raise MbError("crc error in the answer")
        if buf[0] != self.unit:
            raise MbError("answer from unit %d" % buf[0])
        if buf[1] & 0x80:
            raise MbException(buf[2])
        return buf[1:-2]

    def request(self, pdu, resp_len, timeout=0.5, retries=3):
        last = None
        for _ in range(retries):
            try:
                return self._xfer(pdu, resp_len, timeout)
            except MbException:
                raise
            except MbError as e:
                last = e
        raise last

    def read(self, addr, qty, **kw):
        r = self.request(struct.pack(">BHH", 3, addr, qty), 5 + 2 * qty, **kw)
        return list(struct.unpack(">%dH" % qty, r[2:2 + 2 * qty]))

    def write(self, addr, values, **kw):
        if len(values) == 1:
            self.request(struct.pack(">BHH", 6, addr, values[0]), 8, **kw)
        else:
            pdu = struct.pack(">BHHB", 16, addr, len(values), 2 * len(values)) + struct.pack(">%dH" % len(values), *values)
            self.request(pdu, 8, **kw)


def info(m):
    v = m.read(0, 4)
    return {"version": "%d.%d.%d" % (v[0] >> 8, v[0] & 0xFF, v[1]), "uptime_s": v[2], "crashes": v[3]}


def image_version(img):
    if len(img) <= 256 or struct.unpack_from("<I", img, 0)[0] != IMG_MAGIC:
        raise MbError("not an ak-mcu-base image (.img): missing header")
    size = struct.unpack_from("<I", img, 8)[0]
    if 256 + size != len(img):
        raise MbError("image size in the header (%d) does not match the file (%d)" % (256 + size, len(img)))
    return "%d.%d.%d" % (img[20], img[21], img[22]), img[28:44].split(b"\0")[0].decode(errors="replace")


def flash(m, img, progress=None):
    def status():
        st, = m.read(REG_STATUS, 1)
        det, = m.read(REG_DETAIL, 1)
        return "%s (%s)" % (STATUS.get(st, hex(st)), FW_ERRORS.get(det, det))

    total = len(img)
    st, = m.read(REG_STATUS, 1)
    if st == 2:
        raise MbError("board already committed an image and is about to reset - try again in a moment")
    if st == 1:
        m.write(REG_CMD, [CMD_ABORT])           # a session left open by an interrupted run
    try:
        m.write(REG_LEN_HI, [total >> 16, total & 0xFFFF, 0, PSK_IMG >> 16, PSK_IMG & 0xFFFF])
        m.write(REG_CMD, [CMD_BEGIN], timeout=2.0)
        off = 0
        while off < total:
            part = img[off:off + CHUNK]
            if len(part) & 1:
                part += b"\xFF"                 # last register: the board cuts it at the image size
            regs = [off >> 16, off & 0xFFFF] + list(struct.unpack(">%dH" % (len(part) // 2), part))
            m.write(REG_CHUNK, regs, timeout=1.5)   # a 4K sector erase can fall into a chunk
            off += CHUNK
            if progress:
                progress(min(off, total), total)
        # the board reads the whole image back and checks it before answering
        m.write(REG_CMD, [CMD_COMMIT], timeout=10.0, retries=2)
    except MbException as e:
        raise MbError("%s - board status: %s" % (e, status()))


def _progress(done, total):
    sys.stderr.write("\r  %3d%%  %d/%d B" % (done * 100 // total, done, total))
    if done == total:
        sys.stderr.write("\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="serial port of the USB-RS485 adapter (COM16, /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--unit", type=int, default=1, help="slave address 1..247")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info")
    r = sub.add_parser("read")
    r.add_argument("addr", type=lambda x: int(x, 0))
    r.add_argument("count", type=int, nargs="?", default=1)
    w = sub.add_parser("write")
    w.add_argument("addr", type=lambda x: int(x, 0))
    w.add_argument("values", type=lambda x: int(x, 0), nargs="+")
    f = sub.add_parser("flash")
    f.add_argument("image")
    a = ap.parse_args(argv)

    m = Master(a.port, a.baud, a.unit)
    try:
        if a.cmd == "info":
            i = info(m)
            print("unit %d: app v%s, uptime %d s, %d crash record(s)" % (a.unit, i["version"], i["uptime_s"], i["crashes"]))
        elif a.cmd == "read":
            for k, v in enumerate(m.read(a.addr, a.count)):
                print("  %5d: %5d  0x%04X" % (a.addr + k, v, v))
        elif a.cmd == "write":
            m.write(a.addr, a.values)
            print("wrote %d register(s) at %d" % (len(a.values), a.addr))
        elif a.cmd == "flash":
            img = open(a.image, "rb").read()
            ver, board = image_version(img)
            print("unit %d runs v%s; sending v%s (board %s, %d B)" % (a.unit, info(m)["version"], ver, board, len(img)))
            t0 = time.time()
            flash(m, img, _progress)
            print("uploaded and verified on the board in %.1f s, waiting for the restart..." % (time.time() - t0))
            time.sleep(1.0)
            deadline = time.time() + 30
            while True:
                try:
                    i = info(m)
                    if i["uptime_s"] < 20:
                        break
                except MbError:
                    pass
                if time.time() > deadline:
                    raise MbError("board did not come back within 30 s")
                time.sleep(0.3)
            if i["version"] != ver:
                raise MbError("board runs v%s after the update, expected v%s" % (i["version"], ver))
            print("running v%s" % i["version"])
    except MbError as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    finally:
        m.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
