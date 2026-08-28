# RaceChrono AMOLED Monitor — PoC

Firmware for the [Waveshare ESP32-S3-Touch-AMOLED-1.75C](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.75C.htm)
that turns it into a wireless lap-timing display.

```
VBOX Sport ──Bluetooth SPP──▶ phone (RaceChrono) ──BLE──▶ this board
```

The board advertises as a **RaceChrono DIY** BLE device (service `0x1ff8`).
RaceChrono connects, the firmware registers monitor channels (GPS speed +
lap timing), and RaceChrono streams the values, which are rendered with LVGL
on the 466×466 round AMOLED. Kart-focused, delta-first, two swipeable pages:

**Dash page**
- big live delta readout (72px); full-screen background green when faster
  than the comparison lap, red when slower
- outer ring colored by the delta *trend* (green = gaining time, red =
  losing time) — independent of the delta's sign
- current lap time (equal billing with the delta)
- small km/h readout, previous / best lap times (color-coded, no labels)
- lap number & connection status, battery, touch-lock indicator
- pull-down menu (drag from top): screen brightness

**Race page** (swipe left): just the essentials, huge — delta number (110px),
background = delta sign, thick bezel ring = delta trend.

**Controls**: PWR key hold 2 s = power off; BOOT key (5 o'clock) = touch
lock on/off.

## Build & flash

Uses the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork
of `platform-espressif32` (Arduino core 3.x / ESP-IDF 5.x — the official
PlatformIO platform is frozen at core 2.x). FreeRTOS is the native ESP32
Arduino runtime; the firmware runs LVGL as a task pinned to core 1 and the
NimBLE host + RaceChrono config task on core 0.

```sh
pio run                 # build
pio run -t upload       # flash over USB-C
pio device monitor      # serial console (USB CDC)
```

If the board doesn't enumerate for flashing: hold **BOOT**, tap **RESET**,
release BOOT, then upload.

## Pairing with RaceChrono

1. Pair the VBOX Sport with your phone and add it in RaceChrono as usual
   (Settings → Add other device → VBOX Sport).
2. Settings → Add other device → **RaceChrono DIY** → select `RC DIY #xxxx`.
3. Go live (start a session). The display switches from *WAITING FOR
   RACECHRONO* to live data as soon as RaceChrono connects and streams.

No VBOX at hand? RaceChrono's internal phone GPS works too — the display
doesn't care which GPS device feeds RaceChrono.

## Source layout

Rendering and data update are decoupled: `rc_monitor` (NimBLE callbacks) and
the `data_hub` task (core 0) produce a `DashModel`; the LVGL render task
(core 1) consumes it and drives self-contained UI plugins.

| File | Purpose |
| --- | --- |
| `src/main.cpp` | display bring-up (Arduino_GFX CO5300 over QSPI), LVGL init, render task |
| `src/rc_monitor.*` | RaceChrono DIY monitor BLE peripheral (NimBLE), channel config handshake |
| `src/power_mon.*` | AXP2101 PMIC: battery gauge polling + power-key shutdown |
| `src/data_hub.*` | data task (core 0): BLE + PMIC → `DashModel`, derives delta trend |
| `src/dash_model.h` | the model struct UI plugins consume |
| `src/ui_plugin.h` | plugin interface: `create(screen)` + `update(model)` |
| `src/ui.*` | screen composition: plugin registry, `uiTick()` |
| `src/plugin_*.cpp` | one plugin per screen element (delta, trend ring, lap time, speed, prev/best, status, battery) |
| `src/render_num.*` | numeric rendering module: value → display string |
| `src/render_color.*` | color rendering module: value → color (delta sign, trend, battery) |
| `tools/ble_burner.py` | **primary test rig**: Mac-side RaceChrono simulator (see below) |
| `docs/DEVELOPMENT.md` | architecture (plugin system, task split) + bench-testing guide |
| `docs/` | board schematic PDF (from Waveshare's official repo) |
| `include/pin_config.h` | board pin map (from Waveshare's official examples) |
| `include/lv_conf.h` | LVGL 8.4 config (based on Waveshare's, demos off, `millis()` tick) |

## Channels

Registered with RaceChrono at connect time (`src/rc_monitor.cpp`):

| Slot | Equation | Scaling |
| --- | --- | --- |
| 0 | `channel(device(gps), speed)*10.0` | m/s × 10 |
| 1 | `channel(device(lap), lap_number)` | — |
| 2 | `channel(device(lap), lap_time)*10.0` | s × 10 |
| 3 | `channel(device(lap), previous_lap_time)*10.0` | s × 10 |
| 4 | `channel(device(lap), best_lap_time)*10.0` | s × 10 |
| 5 | `channel(device(lap), delta_lap_time)*100.0` | s × 100, signed; invalid until a comparison lap exists |

Any RaceChrono channel (OBD, CAN, accelerometer…) can be added the same way.

## Power

The PWR button is wired to the AXP2101's PWRON pin (see the schematic in
`docs/`), so power-on is pure hardware: press PWR to boot. In firmware,
`src/power_mon.*` polls the PMIC over I2C (no IRQ GPIO is routed):

- **Power off**: short-press the PWR button → clean AXP2101 shutdown (all
  rails cut). Press PWR again to boot.
- **Battery**: percent + charge state shown at the top of the dial,
  color-coded (green charging, amber <30%, red <15%; USB icon when running
  without a battery).

## Next steps (not in the PoC)

- Touch (CST9217 via `SensorLib`) to switch pages
- Live delta-to-best rendering, predictive timing
- AMOLED burn-in care: dim/blank when no data flows

## References

- RaceChrono DIY BLE protocol: <https://github.com/aollin/racechrono-ble-diy-device>
- Board examples & pinout: <https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C>

## Test rig

**Primary: `tools/ble_burner.py` (Mac).** Impersonates RaceChrono from the
Mac — no phone needed:

```sh
pip install bleak
python3 tools/ble_burner.py
```

Streams simulated telemetry at 10 Hz: ~20 s laps, random-walk live delta
±2 s (invalid on lap 1), speed sweep. See `docs/DEVELOPMENT.md` for the
debugging workflow.

### Secondary: `burner/` Android app

An Android app that does the same job for phone-in-the-loop testing: it scans
for the `RC DIY` peripheral, subscribes to the config characteristic, acks
the monitor registration, then streams the same simulated telemetry at 10 Hz.
Android BLE has proven flaky (the app can freeze and stall the stream) —
prefer the Mac script for bench work.

Build (offline, no Gradle — uses SDK aapt2/d8/apksigner directly):

```sh
burner/build.sh
adb install -r burner/build/rc-burner.apk
```

Requires Android 8+ (BLE central). Grant the Bluetooth permission on first
launch, tap SCAN + CONNECT with the board powered on.
