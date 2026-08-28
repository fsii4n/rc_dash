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
  repaint when it does work. The zoomed delta label was removed; big
  readouts come from custom-generated fonts instead (see "Custom fonts").
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
- The flush path is the fast big-endian one: `LV_COLOR_16_SWAP=1` +
  `draw16bitBeRGBBitmap` resolves to `writeBytes`, which DMAs straight out
  of the LVGL draw buffer (no per-pixel swap, no bounce copy), with 16 KB
  SPI transactions (`ESP32QSPI_MAX_PIXELS_AT_ONCE=8192` in platformio.ini).
  Measured effect: full-screen repaints dropped from ~65 ms to ~37 ms.
  Do not switch flush calls back to `draw16bitRGBBitmap` — colors would be
  byte-swapped and the slow path returns.

### Hardware verification status

Touch input, the pull-down menu, the touch-lock button, the 2 s power-off
and the race page were developed **offline** (build-verified only, in
parallel git worktrees by multiple agents, then merged). First on-device
checks worth doing: CST9217 coordinate orientation (`setSwapXY`/
`setMirrorXY` in `touch_input.cpp` are the knobs if mirrored), menu drag
feel, swipe-vs-drag-zone interplay, and the 2 s long-press timing.

## Architecture review: FreeRTOS usage

Reviewed 2026-08: is the current task/snapshot structure the right concurrency
architecture, or should it move to queues, `esp_event`, actors, or a single
loop? **Verdict: keep it.** The decomposition is justified, the data flow's
polling latency is irrelevant at this data rate, and every alternative adds
mechanism without a measurable win for a ~2 KLOC firmware that works. A few
small adjustments are worth making (list at the end); none are refactors.

### Task inventory (verified against source)

| Task | Core | Prio | Stack | Cadence | Why it exists |
| --- | --- | --- | --- | --- | --- |
| NimBLE host | 0 | lib | lib | event | library-owned, not ours to change |
| `rc` (rc_monitor) | 0 | 3 | 4096 | 250 ms | config handshake blocks up to ~3 s per monitor (ack waits) — can't live in a shared loop |
| `pwr` (power_mon) | 0 | 2 | 4096 | 500 ms | I2C transactions, may block on the bus mutex |
| `datahub` | 0 | 2 | 4096 | 50 ms | aggregation + trend EMA + button debounce |
| `lvgl` (main) | 1 | 4 | 8192 | 5 ms loop, 100 ms uiTick | rendering; a full repaint blocks ~65 ms |
| Arduino `loopTask` | 1 | 1 | 8192 | 1 s | idle — does nothing |

The core split is the load-bearing decision and it is right: the one long
blocking operation (the sequential QSPI flush, 16–65 ms) is alone on core 1,
so it can never delay BLE, the PMIC poll, or the config handshake. Priorities
are mostly decorative — every task spends its life in `vTaskDelay`/blocking
I/O, so on core 0 the three tasks almost never contend, and on core 1 the
LVGL task competes only with the idle loopTask. `rc` at 3 vs 2 is harmless
but not meaningful. The trend EMA genuinely benefits from the hub's 20 Hz
sampling (2× the data rate); folding the hub into `uiTick()` would halve it
and couple derivation cadence to render cadence — keeping the task is the
right call, not an accident.

### Data-flow latency (why polling is fine here)

Data arrives at 10 Hz (every 100 ms). Added latency after a BLE write lands
in `RcSnapshot`:

| Stage | Worst | Average |
| --- | --- | --- |
| hub poll (50 ms) | 50 ms | 25 ms |
| uiTick (100 ms) | 100 ms | 50 ms |
| render + flush | 65 ms | ~16 ms |
| **total added** | **~220 ms** | **~90 ms** |

Fully event-driven wiring (task notification from the BLE write → hub →
render) would cut the two poll stages, saving ~75 ms on average — less than
one data period, on a value (lap delta) the driver reads at a glance. The
floor is the render itself, and LVGL is poll-based anyway
(`lv_timer_handler`), so "event-driven" would still funnel into a periodic
refresh. Not worth it unless the delta ever feels laggy on track.

The three copy layers (`sValues` → `RcSnapshot` → `DashModel` → uiTick's
stack copy) look redundant but are each ~40 bytes per 100 ms — they *are*
the cross-core synchronization, with latest-value semantics, which is exactly
what a dash wants. A queue would add history semantics we'd immediately have
to throw away (`xQueueOverwrite` on a length-1 queue is the equivalent idiom,
with more code).

### Alternatives considered

- **Pure superloop** — impossible cleanly: NimBLE owns a host task
  regardless, and the config handshake blocks for seconds. Merging the rest
  into one loop on core 1 would put 65 ms flushes in series with I2C polls
  for zero gain.
- **`esp_event` loop** — pub/sub machinery (event base/id registration,
  handler tables, a dispatcher task) for a fixed pipeline of 3 producers and
  1 consumer carrying periodic telemetry, not sporadic events. All cost, no
  benefit at this scale.
- **Actor / message passing** — same objection, plus queue-depth and
  backpressure decisions that snapshot-overwrite semantics sidestep entirely.
- **Tasks + notifications instead of polling** — the only serious contender;
  see the latency table. Park it behind "only if problems appear".

### Issues found (none critical)

- **`sLvglMutex` is dead weight.** Only `lvglTask` takes it; nothing else
  ever touches LVGL from another task. The one candidate — the brightness
  slider callback — runs inside `lv_timer_handler` on the LVGL task itself
  (same for `gfx->setBrightness()`, which shares the QSPI bus with flushes
  but is only ever called from that task after setup). Remove it, or keep it
  only as `uiLock()`/`uiUnlock()` the day a second caller exists. Corollary
  rule worth stating: **any future cross-task display action (e.g. burn-in
  auto-dim triggered by the data hub) must go through the DashModel and be
  executed by the render task**, not by calling LVGL/gfx from core 0.
- **Ack handshake relies on same-core scheduling.** `sLastAckId` /
  `sLastAckResult` are two separate volatiles written by the NimBLE host task
  and read by `rcTask`. `volatile` orders neither compiler nor cross-core
  visibility; this is correct today only because both tasks are pinned to
  core 0. Pack them into one `volatile uint32_t` (or reuse `sLock`) to make
  it correct by construction.
- **`powerTask` holds the I2C mutex across `delay(100)`** in the shutdown
  path — violating `i2c_bus.h`'s own "never a vTaskDelay" rule. Harmless
  (the board is about to power off) but worth a comment or moving the delay
  out, so the rule stays absolute.
- **Touch-poll vs PMIC-poll contention**: the LVGL indev read (every 10 ms,
  core 1) and the PMIC poll (every 500 ms, core 0) share the I2C mutex, so
  the render task can occasionally stall for the length of one PMIC
  transaction burst (a few ms at 100 kHz I2C). Invisible today; if the 10 s
  frame telemetry ever shows spikes correlating with the 500 ms cadence,
  shorten the PMIC hold (split it into per-register lock/unlock) or raise
  the Wire clock.
- **Serial from five contexts** (NimBLE callbacks, rc, pwr, datahub, lvgl).
  The USB-CDC driver's ring buffer makes concurrent writes safe; worst case
  is interleaved log lines. Cosmetic, leave it.
- **`sState` can flip CONFIGURING→STREAMING mid-handshake** if a data write
  arrives before all six monitors are acked. Cosmetic (status text only);
  real centrals don't stream before configuration completes.
- **Stack sizes are guesses**, not measurements — 4096/8192 are the usual
  Arduino reflexes. Measure before trusting or trimming: log
  `uxTaskGetStackHighWaterMark(NULL)` (remaining words, worst case since
  boot) from each task alongside the existing 10 s frame telemetry, run a
  full bench session with connects/disconnects and menu use, and keep
  ~25 % headroom above the observed minimum.

### Adjustments worth making (prioritized)

1. **Delete `sLvglMutex`** (or rename into a real `uiLock()` API only when a
   second caller appears) and add the "display actions go through DashModel"
   rule as a comment. ~10 min.
2. **Add stack high-water telemetry** to the 10 s report; adjust stack sizes
   from data. ~20 min + one bench session.
3. **Make the config ack race-proof**: single packed volatile (or take
   `sLock`) for id+result. ~10 min.
4. **Comment or fix the `delay(100)`-under-mutex** in `powerTask`'s shutdown
   path. ~5 min.
5. *Only if problems appear:* event-driven data path (BLE write → task
   notification → hub → notification → render) if the delta ever feels
   laggy — saves ~75 ms average, ~150 ms worst. ~half a day incl. re-test.
6. *Only if problems appear:* split the PMIC I2C burst into shorter mutex
   holds if frame telemetry shows 500 ms-period spikes. ~30 min.
7. *Optional housekeeping:* `loopTask` idles with an 8 KB stack; reclaiming
   it (`vTaskDelete(NULL)` at the end of the first `loop()` pass) frees the
   RAM. Only bother if memory gets tight.

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
