#!/usr/bin/env python3
"""
ak_plot.py - feed numbers to the "Scope" screen of the AK Base Kit demo.

  ak_plot.py --port COM5 sine                 # over the console UART (shell command "plot")
  ak_plot.py --port COM16 --rs485 sine        # over RS485: Modbus, holding register 16
  some_program | ak_plot.py --port COM5 stdin # one number per line from another program

Sources: sine, noise (a random walk), stdin. Values are sent as signed 16-bit
integers; stdin numbers may have decimals and are rounded (scale them first
with --gain if they are small).

Needs pyserial.
"""

import argparse
import math
import os
import random
import sys
import time

PLOT_REGISTER = 16


def source_sine():
    t = 0
    while True:
        yield 1000 * math.sin(t / 9.0) + 250 * math.sin(t / 2.3)
        t += 1


def source_noise():
    v = 0.0
    while True:
        v = max(-2000, min(2000, v + random.uniform(-120, 120)))
        yield v


def source_stdin():
    for line in sys.stdin:
        line = line.strip().replace(",", " ").split()
        if line:
            try:
                yield float(line[0])
            except ValueError:
                pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", choices=["sine", "noise", "stdin"])
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=None, help="default 115200 (console) or 9600 (--rs485)")
    ap.add_argument("--rs485", action="store_true", help="send by Modbus (register 16) instead of the shell")
    ap.add_argument("--unit", type=int, default=1, help="Modbus address of the kit (default 1)")
    ap.add_argument("--rate", type=float, default=20.0, help="points per second (default 20)")
    ap.add_argument("--gain", type=float, default=1.0, help="multiply every value by this")
    ap.add_argument("-t", "--seconds", type=float, default=0.0, help="stop after this long (default: run until Ctrl+C)")
    a = ap.parse_args()

    values = {"sine": source_sine, "noise": source_noise, "stdin": source_stdin}[a.source]()
    sent = failed = 0
    if a.rs485:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import ak_mb

        master = ak_mb.Master(a.port, a.baud or 9600, a.unit)

        def send(v):
            master.write(PLOT_REGISTER, [v & 0xFFFF])
        close = master.close
    else:
        try:
            import serial
        except ImportError:
            raise SystemExit("pyserial required: pip install pyserial")
        port = serial.Serial(a.port, a.baud or 115200, timeout=0)

        def send(v):
            port.read(4096)                     # the echo and the prompt are not needed
            port.write(("plot %d\r" % v).encode())
        close = port.close

    t0 = time.time()
    try:
        for x in values:
            v = int(round(max(-32768, min(32767, x * a.gain))))
            try:
                send(v)
                sent += 1
            except Exception as e:              # a lost Modbus answer must not stop the stream
                failed += 1
                if failed == 1:
                    sys.stderr.write("send failed: %s\n" % e)
            if a.seconds and time.time() - t0 >= a.seconds:
                break
            wait = t0 + (sent + failed) / a.rate - time.time()
            if wait > 0:
                time.sleep(wait)
    except KeyboardInterrupt:
        pass
    finally:
        close()
    print("%d point(s) sent in %.1f s, %d failed" % (sent, time.time() - t0, failed))
    sys.exit(0 if sent and not failed else 1)


if __name__ == "__main__":
    main()
