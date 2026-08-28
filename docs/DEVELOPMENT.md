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
- **`ui.cpp`** — screen composition only: a `PluginSlot {plugin, page}`
  registry plus `uiCreate()`/`uiTick()`. The UI is a two-page `lv_tileview`
  (horizontal swipe, snap): page 0 is the full dash, page 1 the minimal
  race page (`plugin_race_page`: thick trend ring + 110px delta only).
  The tileview and tiles are transparent so the screen background — the
  delta sign color set by `plugin_delta` on `lv_scr_act()` — shows through
  on both pages. Every plugin updates every tick regardless of visibility;
  change-caches keep the off-screen page free. `kPluginMenu` stays last in
  the registry (its panel must be created on top); the menu lives in tile 0
  only. Adding/removing a screen element = one registry line.
- **`touch_input`** — CST9217 via SensorLib (`TouchDrvCST92xx`), registered
  as an LVGL pointer indev; the read callback swallows touches while
  `dataHubGetTouchLocked()` is set (BOOT key toggles it).
- **`i2c_bus`** — the Wire bus is shared (AXP2101 on core 0, CST9217 from
  the LVGL task on core 1) and Wire is not thread-safe: EVERY I2C
  transaction must sit inside `i2cBusLock()`/`i2cBusUnlock()`. Any new I2C
  device (IMU, etc.) must follow this rule.
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

### Controls

- **PWR key** (1 o'clock, AXP2101 PWRON — not a GPIO): hold **2 s** to power
  off (`XPOWERS_AXP2101_PKEY_LONG_IRQ` + `setIrqLevelTime(2S)`, polled every
  500 ms). Short press does nothing (IRQ not enabled, reserved). Hardware
  OFFLEVEL failsafe stays at 4 s in case firmware hangs.
- **BOOT key** (5 o'clock, GPIO0 active-low, verified against the schematic
  netlist): toggles the touch lock. Debounced by two consecutive 50 ms polls
  in the data_hub task. Locked state shows `LV_SYMBOL_EYE_CLOSE` (LVGL 8.4
  has no lock glyph) in amber at the bottom of the dial.
- **Pull-down menu**: drag >40 px down from the top-edge zone (dash page
  only) → brightness slider (5..255 → `displaySetBrightness()`), close via
  button or tapping the scrim. The scrim blocks clicks and scroll-chaining
  so the open menu can't swipe pages.
- **Page swipe**: horizontal swipe anywhere flips dash ⇄ race page
  (tileview snap).

### Custom fonts

Built-in Montserrat stops at 48pt. Bigger readouts use fonts generated with
`lv_font_conv` (node/npx) from the TTF LVGL ships in
`.pio/libdeps/amoled-175c/lvgl/scripts/built_in_font/`:

```sh
npx lv_font_conv --font Montserrat-Medium.ttf --size 110 --bpp 4 \
  --format lvgl --range 0x2B-0x3A -o src/font_montserrat_num_110.c \
  --force-fast-kern-format --no-compress
```

Digits-only (`+,-./0-9:`) keeps flash small: the 72px font costs ~12 KB, the
110px one ~28 KB. Fix the generated include to `<lvgl.h>` (the project uses
`LV_CONF_INCLUDE_SIMPLE`); declare with `LV_FONT_DECLARE(...)` at use sites.
Never scale fonts with transform styles (see below).

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

### Hardware verification status

Touch input, the pull-down menu, the touch-lock button, the 2 s power-off
and the race page were developed **offline** (build-verified only, in
parallel git worktrees by multiple agents, then merged). First on-device
checks worth doing: CST9217 coordinate orientation (`setSwapXY`/
`setMirrorXY` in `touch_input.cpp` are the knobs if mirrored), menu drag
feel, swipe-vs-drag-zone interplay, and the 2 s long-press timing.

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
