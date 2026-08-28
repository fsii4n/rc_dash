#!/usr/bin/env python3
"""Mac-side test rig: impersonates RaceChrono against the rc_dash firmware.

This is the primary bench-test tool (the Android burner app under burner/
works too, but Android BLE has proven flaky — the app can freeze and stall
the stream, which looks like a frozen display on the board).

Scans for the "RC DIY" peripheral (service 0x1ff8), subscribes to the config
characteristic, acks the monitor registration, then streams simulated
telemetry at 10 Hz: ~20s laps, a random-walk live delta clamped to ±2s
(invalid on lap 1, reset to 0 at each lap crossing), speed sweeping
40–250 km/h.

Usage:
    pip install bleak
    python3 tools/ble_burner.py

Stop with Ctrl-C. Watch the firmware side with `pio device monitor`.
"""
import asyncio
import math
import random
import struct
import sys
import time

from bleak import BleakClient, BleakScanner

SERVICE = "00001ff8-0000-1000-8000-00805f9b34fb"
CONFIG_CHAR = "00000005-0000-1000-8000-00805f9b34fb"
NOTIFY_CHAR = "00000006-0000-1000-8000-00805f9b34fb"

CMD_REMOVE_ALL = 0
CMD_REMOVE = 1
CMD_ADD_INCOMPLETE = 2
CMD_ADD = 3
RESULT_OK = 0
RESULT_SEQ_ERR = 1  # payload out of sequence -> firmware resends

INVALID = 0x7FFFFFFF
SIM_PERIOD = 0.1  # 10 Hz, like a real GPS feed

# Fallback when an equation arrives truncated (dropped indication part):
# the firmware's registration order from src/rc_monitor.cpp.
ID_ORDER = ["speed", "lap_number", "lap_time", "previous_lap_time",
            "best_lap_time", "delta_lap_time"]

# Substring match, most specific first ("delta_lap_time" and
# "previous_lap_time" both contain "lap_time").
MATCH_ORDER = ["delta_lap_time", "previous_lap_time", "best_lap_time",
               "lap_time", "lap_number", "speed"]


def channel_key(monitor_id: int, equation: str) -> str:
    for key in MATCH_ORDER:
        if key in equation:
            return key
    if monitor_id < len(ID_ORDER):
        print(f"warn: monitor {monitor_id} equation unrecognized "
              f"({equation!r}), assuming {ID_ORDER[monitor_id]}", flush=True)
        return ID_ORDER[monitor_id]
    return ""


class Sim:
    """Mirrors the Android burner 0.2 simulation."""

    def __init__(self):
        self.start = time.time()
        self.lap_start = self.start
        self.lap_num = 1
        self.lap_dur = 19.5
        self.prev_deci = INVALID
        self.best_deci = INVALID
        self.delta_cs = 0

    def tick(self):
        now = time.time()
        if now - self.lap_start >= self.lap_dur:
            self.prev_deci = int(self.lap_dur * 10)
            self.best_deci = (self.prev_deci if self.best_deci == INVALID
                              else min(self.best_deci, self.prev_deci))
            self.lap_num += 1
            self.lap_start = now
            self.lap_dur = 18 + random.random() * 4
            self.delta_cs = 0
            print(f"sim: lap {self.lap_num}", flush=True)
        # Smooth random walk, clamped to ±2s
        self.delta_cs = max(-200, min(200, self.delta_cs + random.randint(-8, 8)))
        kmh = 145 + 105 * math.sin((now - self.start) / 20 * 2 * math.pi)
        return {
            "speed": int(kmh / 3.6 * 10),
            "lap_number": self.lap_num,
            "lap_time": int((now - self.lap_start) * 10),
            "previous_lap_time": self.prev_deci,
            "best_lap_time": self.best_deci,
            "delta_lap_time": INVALID if self.lap_num <= 1 else self.delta_cs,
        }


async def main():
    print("scanning for RC DIY...", flush=True)
    dev = await BleakScanner.find_device_by_filter(
        lambda d, ad: SERVICE in (ad.service_uuids or []), timeout=15)
    if not dev:
        print("RC DIY peripheral not found (is the board powered on?)",
              flush=True)
        sys.exit(1)
    print(f"found {dev.name} ({dev.address})", flush=True)

    async with BleakClient(dev) as client:
        print("connected", flush=True)
        loop = asyncio.get_running_loop()
        channels = {}   # monitor id -> channel key
        parts = {}      # monitor id -> [payload parts]
        broken = set()  # monitor ids with a dropped/mis-ordered part

        def on_config(_char, data: bytearray):
            if len(data) < 3:
                return
            cmd, mid, seq = data[0], data[1], data[2]
            payload = bytes(data[3:])
            if cmd == CMD_REMOVE_ALL:
                channels.clear()
                print("cfg: remove all", flush=True)
            elif cmd == CMD_ADD_INCOMPLETE:
                if seq == 0:
                    parts[mid] = []
                    broken.discard(mid)
                elif len(parts.get(mid, [])) != seq:
                    broken.add(mid)
                parts.setdefault(mid, []).append(payload)
            elif cmd == CMD_ADD:
                # NAK on a dropped part so the firmware resends the monitor.
                if mid in broken or (seq > 0 and len(parts.get(mid, [])) != seq):
                    print(f"warn: monitor {mid} payload out of sequence, "
                          "NAKing for resend", flush=True)
                    parts.pop(mid, None)
                    broken.discard(mid)
                    loop.create_task(client.write_gatt_char(
                        CONFIG_CHAR, bytes([RESULT_SEQ_ERR, mid]),
                        response=True))
                    return
                eq = (b"".join(parts.pop(mid, [])) + payload).decode(
                    "utf-8", "replace")
                channels[mid] = channel_key(mid, eq)
                print(f"cfg: monitor {mid} = {eq}", flush=True)
                loop.create_task(client.write_gatt_char(
                    CONFIG_CHAR, bytes([RESULT_OK, mid]), response=True))
            elif cmd == CMD_REMOVE:
                channels.pop(mid, None)
                print(f"cfg: remove {mid}", flush=True)

        await client.start_notify(CONFIG_CHAR, on_config)
        print("subscribed, waiting for monitor config...", flush=True)

        sim = Sim()
        last_status = 0.0
        while client.is_connected:
            await asyncio.sleep(SIM_PERIOD)
            vals = sim.tick()
            out = b"".join(
                struct.pack(">Bi", mid, vals[key])
                for mid, key in sorted(channels.items()) if key)
            if out:
                try:
                    await client.write_gatt_char(NOTIFY_CHAR, out,
                                                 response=False)
                except Exception as e:  # connection dropped mid-write
                    print(f"write failed ({e}), exiting", flush=True)
                    break
            if time.time() - last_status >= 5:
                last_status = time.time()
                d = vals["delta_lap_time"]
                print(f"lap {vals['lap_number']}  t {vals['lap_time'] / 10:5.1f}s  "
                      f"delta {'--' if d == INVALID else f'{d / 100:+.2f}s'}  "
                      f"{vals['speed'] * 0.36:3.0f} km/h", flush=True)
        print("disconnected", flush=True)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
