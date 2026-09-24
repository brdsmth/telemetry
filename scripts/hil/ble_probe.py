#!/usr/bin/env python3
"""Reference BLE central for the sync service (schema/PROTOCOL.md §3).

Scans for sensors advertising the telemetry service, decodes their
manufacturer data, connects to one, reads Device Info, opens a session, sets
the time, pulls records from a seq, verifies every chunk's CRC and seq
continuity, and optionally acknowledges. This is the host-side counterpart of
the firmware's SyncSession and the model the app's CollectionService follows.

Examples:
  ble_probe.py scan
  ble_probe.py pull                       # nearest sensor, from collected_through + 1
  ble_probe.py pull --from 1 --max 50
  ble_probe.py pull --ack-collected       # ack what was received
  ble_probe.py pull --ack-secured 120     # server confirmed through seq 120

Needs bleak (`pip install bleak`). PlatformIO's python has it if installed there:
  /opt/homebrew/Cellar/platformio/<ver>/libexec/bin/python scripts/hil/ble_probe.py scan
"""
from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import time
import uuid

try:
    from bleak import BleakClient, BleakScanner  # type: ignore
except ImportError:  # pragma: no cover
    print("bleak not available; pip install bleak", file=sys.stderr)
    sys.exit(2)

SERVICE_UUID = "6eaedc54-f770-40e3-9806-a6ccf63c8099"
CHAR_INFO = "6eaedc54-f770-40e3-0001-a6ccf63c8099"
CHAR_CONTROL = "6eaedc54-f770-40e3-0002-a6ccf63c8099"
CHAR_DATA = "6eaedc54-f770-40e3-0003-a6ccf63c8099"
CHAR_STATUS = "6eaedc54-f770-40e3-0004-a6ccf63c8099"
MANUFACTURER_ID = 0xFFFF

RECORD_SIZE = 20
CHUNK_HEADER = 8
CHUNK_LAST = 0x01

OP_OPEN, OP_SET_TIME, OP_READ_FROM, OP_ACK_COLLECTED, OP_ACK_SECURED, OP_CLOSE = 1, 2, 3, 4, 5, 6
RESULTS = {0: "ok", 1: "no_session", 2: "bad_argument", 3: "seq_out_of_range", 4: "busy"}
QUALITY = {0: "ok", 1: "low", 2: "high", 3: "open"}
TYPES = {1: "soil_resistance_ohms", 2: "battery_millivolts", 3: "board_temperature_c"}


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def decode_record(b: bytes) -> dict:
    if len(b) < RECORD_SIZE:
        raise ValueError("short_input")
    if b[0] != 1:
        raise ValueError("unsupported_version")
    if crc16(b[:18]) != struct.unpack_from("<H", b, 18)[0]:
        raise ValueError("bad_crc")
    version, typ, quality, flags, seq, t, value, boot_id = struct.unpack_from("<BBBBIIfH", b, 0)
    return {
        "seq": seq, "type": TYPES.get(typ, typ), "quality": QUALITY.get(quality, quality),
        "epoch_valid": bool(flags & 1), "time": t, "value": value, "boot_id": boot_id,
    }


def decode_device_info(b: bytes) -> dict:
    f = struct.unpack("<BB6sHIIIIIIHH16s", b[:54])
    return {
        "protocol_version": f[0], "record_size": f[1], "device_id": f[2].hex(), "boot_id": f[3],
        "next_seq": f[4], "collected_through": f[5], "secured_through": f[6], "uptime_s": f[7],
        "unix_time": f[8], "dropped": f[9], "battery_mv": f[10], "capacity": f[11],
        "fw_version": f[12].rstrip(b"\0").decode("ascii", "replace"),
    }


def decode_advertising(mfg: dict) -> dict | None:
    payload = mfg.get(MANUFACTURER_ID)
    if not payload or len(payload) < 6:
        return None
    pv, pending, battery, flags = struct.unpack_from("<BHHB", payload, 0)
    return {"protocol_version": pv, "pending": pending, "battery_mv": battery,
            "time_set": bool(flags & 1), "dropped": bool(flags & 2)}


async def scan(seconds: float) -> list:
    found = {}

    def cb(device, adv):
        if SERVICE_UUID in [u.lower() for u in adv.service_uuids]:
            found[device.address] = (device, adv)

    scanner = BleakScanner(cb, service_uuids=[SERVICE_UUID])
    await scanner.start()
    await asyncio.sleep(seconds)
    await scanner.stop()
    out = sorted(found.values(), key=lambda da: -(da[1].rssi or -200))
    for device, adv in out:
        info = decode_advertising(adv.manufacturer_data) or {}
        print(f"{device.address}  {adv.local_name or device.name or '?':<12} rssi {adv.rssi:>4}  {info}")
    if not out:
        print("no telemetry sensors seen")
    return out


class Central:
    def __init__(self, client: BleakClient):
        self.client = client
        self.status_q: asyncio.Queue = asyncio.Queue()
        self.chunk_q: asyncio.Queue = asyncio.Queue()

    async def start(self):
        await self.client.start_notify(CHAR_STATUS, lambda _, d: self.status_q.put_nowait(bytes(d)))
        await self.client.start_notify(CHAR_DATA, lambda _, d: self.chunk_q.put_nowait(bytes(d)))

    async def command(self, payload: bytes, timeout: float = 5.0) -> tuple[int, str, int]:
        await self.client.write_gatt_char(CHAR_CONTROL, payload, response=True)
        raw = await asyncio.wait_for(self.status_q.get(), timeout)
        opcode, result, seq = struct.unpack("<BBI", raw[:6])
        name = RESULTS.get(result, str(result))
        print(f"  cmd 0x{payload[0]:02x} -> {name} seq {seq}")
        if result != 0:
            raise RuntimeError(f"command 0x{payload[0]:02x} failed: {name}")
        return opcode, name, seq

    async def device_info(self) -> dict:
        return decode_device_info(await self.client.read_gatt_char(CHAR_INFO))

    async def pull(self, from_seq: int, max_records: int, timeout: float = 10.0) -> list[dict]:
        while not self.chunk_q.empty():
            self.chunk_q.get_nowait()
        await self.command(struct.pack("<BIH", OP_READ_FROM, from_seq, max_records))
        records: list[dict] = []
        expected = None
        chunks = 0
        t0 = time.monotonic()
        while True:
            raw = await asyncio.wait_for(self.chunk_q.get(), timeout)
            first_seq, count, flags, crc = struct.unpack_from("<IBBH", raw, 0)
            body = raw[CHUNK_HEADER:]
            if len(body) != count * RECORD_SIZE:
                raise RuntimeError(f"chunk body {len(body)} bytes for count {count}")
            if crc16(body) != crc:
                raise RuntimeError(f"chunk crc mismatch at first_seq {first_seq}")
            if expected is not None and count and first_seq != expected:
                print(f"  gap: expected seq {expected}, chunk starts at {first_seq}")
            for i in range(count):
                r = decode_record(body[i * RECORD_SIZE:(i + 1) * RECORD_SIZE])
                if r["seq"] != first_seq + i:
                    raise RuntimeError(f"record seq {r['seq']} not consecutive in chunk from {first_seq}")
                records.append(r)
            chunks += 1
            expected = first_seq + count
            if flags & CHUNK_LAST:
                break
        dt = time.monotonic() - t0
        rate = (len(records) * RECORD_SIZE / dt) if dt else 0
        print(f"  {len(records)} records in {chunks} chunks, {dt:.2f}s, {rate:.0f} B/s")
        return records


async def run_pull(args) -> int:
    devices = await scan(args.scan_seconds)
    if not devices:
        return 1
    device = devices[0][0] if not args.address else next(
        (d for d, _ in devices if d.address.lower() == args.address.lower()), None)
    if device is None:
        print(f"{args.address} not seen", file=sys.stderr)
        return 1

    print(f"connecting to {device.address} ...")
    async with BleakClient(device, timeout=20.0) as client:
        central = Central(client)
        await central.start()
        mtu = getattr(client, "mtu_size", None)
        print(f"connected, mtu {mtu}")

        info = await central.device_info()
        print("device info:", info)
        if info["protocol_version"] != 1:
            print("unsupported protocol version", file=sys.stderr)
            return 1

        session_id = uuid.uuid4()
        print(f"session {session_id}")
        await central.command(struct.pack("<B", OP_OPEN) + session_id.bytes)

        if not args.no_set_time:
            now = int(time.time())
            uptime_at_connect = info["uptime_s"]
            await central.command(struct.pack("<BI", OP_SET_TIME, now))
        else:
            now, uptime_at_connect = None, None

        from_seq = args.from_seq if args.from_seq is not None else info["collected_through"] + 1
        records = await central.pull(from_seq, args.max)

        for r in records[: args.show]:
            when = r["time"]
            if not r["epoch_valid"] and now is not None and r["boot_id"] == info["boot_id"]:
                when = f"{now - uptime_at_connect + r['time']} (backfilled)"
            print(f"    seq {r['seq']:>6}  {r['type']:<22} {r['value']:>10.1f}  {r['quality']:<5} "
                  f"t={when} boot={r['boot_id']}")
        if len(records) > args.show:
            print(f"    ... {len(records) - args.show} more")

        if records and args.ack_collected:
            await central.command(struct.pack("<BI", OP_ACK_COLLECTED, records[-1]["seq"]))
        if args.ack_secured is not None:
            await central.command(struct.pack("<BI", OP_ACK_SECURED, args.ack_secured))

        info = await central.device_info()
        print("device info after:", {k: info[k] for k in ("next_seq", "collected_through", "secured_through", "dropped")})
        await central.command(struct.pack("<B", OP_CLOSE))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("scan", help="list advertising sensors")
    s.add_argument("--scan-seconds", type=float, default=5.0)

    p = sub.add_parser("pull", help="connect and pull records")
    p.add_argument("--address", help="sensor address; default is the strongest signal")
    p.add_argument("--scan-seconds", type=float, default=5.0)
    p.add_argument("--from", dest="from_seq", type=int, help="first seq; default collected_through + 1")
    p.add_argument("--max", type=int, default=0, help="max records, 0 = all")
    p.add_argument("--show", type=int, default=10, help="records to print")
    p.add_argument("--no-set-time", action="store_true")
    p.add_argument("--ack-collected", action="store_true", help="ack the last received seq as collected")
    p.add_argument("--ack-secured", type=int, help="ack through this seq as secured (frees slots)")

    args = ap.parse_args()
    if args.cmd == "scan":
        return 0 if asyncio.run(scan(args.scan_seconds)) else 1
    return asyncio.run(run_pull(args))


if __name__ == "__main__":
    sys.exit(main())
