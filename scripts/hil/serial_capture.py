#!/usr/bin/env python3
"""Reset an ESP32 over its serial port and capture its output for a while.

The first rung of the hardware-in-the-loop tier in docs/TESTING.md: no
assertions, just a deterministic capture that other scripts and humans can
grep. Refuses to fight another process for the port.

Usage:
  serial_capture.py [--port /dev/cu.usbserial-0001] [--baud 115200]
                    [--seconds 60] [--no-reset] [--out capture.log]

Needs pyserial. PlatformIO ships one:
  /opt/homebrew/Cellar/platformio/<ver>/libexec/bin/python scripts/hil/serial_capture.py
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    print("pyserial not available; run with PlatformIO's python or `pip install pyserial`", file=sys.stderr)
    sys.exit(2)


def port_in_use(port: str) -> str | None:
    """Returns the process line holding the port, if any (macOS/Linux lsof)."""
    try:
        out = subprocess.run(["lsof", port], capture_output=True, text=True, timeout=5).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return None
    lines = out.splitlines()
    return lines[1] if len(lines) > 1 else None


def reset(ser: serial.Serial) -> None:
    """Pulse EN via RTS the way esptool and the PlatformIO monitor do."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.1)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="/dev/cu.usbserial-0001")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--no-reset", action="store_true", help="attach without resetting the board")
    ap.add_argument("--out", help="also write the capture to this file")
    args = ap.parse_args()

    holder = port_in_use(args.port)
    if holder:
        print(f"{args.port} is held by another process, not attaching:\n  {holder}", file=sys.stderr)
        return 3

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
    except serial.SerialException as e:
        print(f"open {args.port}: {e}", file=sys.stderr)
        return 1

    sink = open(args.out, "w", encoding="utf-8", errors="replace") if args.out else None
    try:
        if not args.no_reset:
            reset(ser)
            ser.reset_input_buffer()
        start = time.monotonic()
        buf = b""
        while time.monotonic() - start < args.seconds:
            chunk = ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                stamp = f"{time.monotonic() - start:7.2f}"
                print(f"[{stamp}] {text}", flush=True)
                if sink:
                    sink.write(f"[{stamp}] {text}\n")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        if sink:
            sink.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
