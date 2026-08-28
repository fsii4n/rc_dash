# Development guide

## Architecture: modular plugins, decoupled data/render

Rendering and data update are fully decoupled and run as separate FreeRTOS
tasks on separate cores:

```
core 0                                    core 1
──────────────────────────────────────    ─────────────────────────────────
NimBLE host ──▶ rc_monitor (RcSnapshot)   LVGL render task (main.cpp)
AXP2101 poll ─▶ power_mon  (PowerStatus)    └─ uiTick() every 100 ms:
                    │                            dataHubGet(model)
data_hub task (20 Hz):                           for each plugin:
  aggregate + derive ──▶ DashModel ─────────────▶  plugin->update(model)
```

- **`rc_monitor`** — RaceChrono DIY BLE peripheral. Owns the protocol; writes
  raw channel values into a thread-safe `RcSnapshot`. Knows nothing about UI.
- **`data_hub`** — FreeRTOS task (core 0) that merges the BLE snapshot and
  PMIC readings into one **`DashModel`** and computes *derived* values —
  currently the delta trend (EMA-smoothed derivative of the live delta;
  lap-crossing jumps are clamped out). All derived-data logic belongs here,
  never in a plugin.
- **UI plugins** (`src/plugin_*.cpp`) — one self-contained module per screen
  element. A plugin implements the two-function `UiPlugin` interface from
  `ui_plugin.h`:
  - `create(screen)` — build & position its LVGL objects
  - `update(model)` — map the latest `DashModel` onto them
  Plugins never touch BLE, FreeRTOS, or other plugins.
- **`ui.cpp`** — screen composition only: the plugin registry array plus
  `uiCreate()`/`uiTick()`. Adding/removing a screen element = one line here.
- **Shared render modules** — plugins do not hand-roll formatting or colors:
  - `render_num` — quantitative value → display string (lap times, delta
    seconds, km/h). Pure functions, no LVGL. Invalid values render as
    placeholders.
  - `render_color` — value → color rules (delta sign → green/red background,
    delta trend → ring color, battery state). One palette for the screen.

### Adding a screen element

1. Create `src/plugin_foo.cpp`: implement `create`/`update`, export
   `extern const UiPlugin kPluginFoo = {"foo", create, update};`
2. Add `extern const UiPlugin kPluginFoo;` + a registry entry in `ui.cpp`.
3. Need a new value? Add the channel in `rc_monitor.cpp` (`kMonitors`) and
   `rc_monitor.h` (`RcChannel`), or a derived value in `data_hub.cpp` +
   `dash_model.h`. The Android burner's equation matcher
   (`burner/.../MainActivity.java`) must recognize the new equation — keep
   more specific substrings matched first (`delta_lap_time` before
   `lap_time`).

### Protocol hardening

Monitor registration waits for the central's per-ADD result write and resends
up to 3 times on timeout or a `PAYLOAD_OUT_OF_SEQUENCE` result
(`rc_monitor.cpp`, `addMonitorAcked`). Rationale: BLE indications' payload
parts were observed getting dropped with both Android and macOS centrals,
silently corrupting the registered equations (channel goes dead).
`tools/ble_burner.py` NAKs out-of-sequence payloads (result code 1) to
trigger the resend.

### Rendering rules of thumb

- Only touch LVGL state when the value actually changed — every
  `lv_label_set_text`/`lv_obj_set_style_*` call invalidates and forces a
  redraw. Use `uiSetLabelText()` for labels; cache the previous value for
  styles (see `plugin_delta.cpp`, `plugin_trend_ring.cpp`).
- **Avoid `lv_obj_set_style_transform_zoom` on widgets entirely.** On LVGL
  8.4 a transformed widget renders through a draw layer, which (a) silently
  draws *nothing* when the layer buffer can't be allocated — the zoomed
  delta number was invisible because of this — and (b) costs 60 ms+ per
  repaint when it does work. The zoomed delta label was removed; the delta
  readout is now a plain `montserrat_48` label, same as the lap time.
- `LV_MEM_CUSTOM` must stay **0**. Switching LVGL to system malloc
  (`LV_MEM_CUSTOM 1`) crash-looped the board at startup — boot stopped
  between the PMIC init and BLE advertising, with USB CDC re-enumerating.
  The 48 KB static pool is fine now that no draw layers are used.

### Display performance

- Full-screen repaints (background color flips) measure ~65 ms worst-case vs
  ~16 ms for normal frames. Frame telemetry prints
  `[ui] max frame Nms over last 10s` every 10 s on serial.
- The delta background flip has ±0.10 s hysteresis (`plugin_delta.cpp`) so a
  delta wobbling around zero doesn't repaint the whole screen every tick.
- The visible top-to-bottom wipe on full repaints comes from sequential
  quarter-screen blocking SPI flushes. A faster flush path is being worked
  on: `LV_COLOR_16_SWAP` + a `draw16bitBeRGBBitmap`/`writeBytes` direct
  (big-endian) path + larger SPI transactions via
  `ESP32QSPI_MAX_PIXELS_AT_ONCE`.

## Bench testing: stream from the Mac

**Use `tools/ble_burner.py` as the primary test rig.** It impersonates
RaceChrono from the Mac: scans for the board, acks the monitor registration,
then streams simulated telemetry at 10 Hz (~20 s laps, random-walk delta
±2 s, invalid on lap 1).

```sh
pip install bleak            # once (any Python 3.9+ env)
python3 tools/ble_burner.py  # Ctrl-C to stop
```

The Android app under `burner/` does the same job but has proven flaky:
Android BLE / the app can freeze and silently stall the stream, which looks
exactly like a frozen display on the board and wastes debugging time. Prefer
the Mac script; keep the app only for phone-in-the-loop testing.

### Watching the firmware

```sh
pio device monitor           # interactive serial console (USB CDC)
```

The firmware logs BLE lifecycle events (`central connected`,
`monitors configured`) and a periodic sample of incoming data writes
(`data write #N, 30 bytes: ...` — first write, then every 50th), which is the
fastest way to tell "no data arriving" apart from "UI not rendering".

Non-interactive capture (agents, scripts) — `pio device monitor` needs a TTY;
read the port directly instead, e.g.:

```python
import serial
s = serial.Serial('/dev/cu.usbmodemXXX', 115200, timeout=1)
# optional hard reset, same as pio's "Hard resetting via RTS pin":
s.rts = True; import time; time.sleep(0.1); s.rts = False
while True: print(s.read(4096).decode('utf-8', 'replace'), end='')
```

### Diagnosis cheat-sheet

| Symptom | Meaning |
| --- | --- |
| Display shows placeholders, status `WAITING FOR RACECHRONO` | No central connected |
| Status `CONNECTED, CONFIGURING...` stuck | Central connected but never wrote data (check the test rig actually streams) |
| Status `LAP n`, but delta/PREV/BEST placeholders on lap 1 | **Normal** — no comparison lap yet; wait one lap (~20 s with the sim) |
| Serial shows `data write #N` but screen frozen | Render-side problem (LVGL) — check heap, rendering notes above |
| Serial silent, screen frozen | Board hung/crashed — reset and watch boot logs |
