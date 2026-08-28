// Screen composition: the plugin registry. Each element of the 466x466
// round AMOLED is a self-contained UiPlugin; this file only decides which
// plugins exist, which page (tileview tile) each one lives on, and the order
// they update in (delta owns the screen background, so it runs early).
//
// Two horizontally-swipeable pages:
//   page 0 — the full kart dash, top to bottom:
//     - outer ring ....... delta trend (green = gaining, red = losing)
//     - battery .......... top of the dial
//     - speed ............ small km/h readout
//     - live delta ....... huge, screen bg green/red by its sign
//     - current lap time . equal billing with the delta
//     - prev/best row
//     - status ........... lap number / connection state
//     - lock ............. touch-lock indicator (BOOT key toggles)
//   page 1 — the race page: only the delta sign color (screen bg), the
//     delta trend (thick bezel ring), and the delta number, huge.
#include "ui.h"

#include "data_hub.h"
#include "ui_plugin.h"

extern const UiPlugin kPluginTrendRing;
extern const UiPlugin kPluginDelta;
extern const UiPlugin kPluginLapTime;
extern const UiPlugin kPluginSpeed;
extern const UiPlugin kPluginLaps;
extern const UiPlugin kPluginStatus;
extern const UiPlugin kPluginBattery;
extern const UiPlugin kPluginLock;
extern const UiPlugin kPluginRacePage;
extern const UiPlugin kPluginMenu;

// Registry entry: which plugin, and which page (tile column) its objects are
// created on. The UiPlugin interface itself is unchanged — plugins still
// just receive a full-screen-sized parent.
struct PluginSlot {
  const UiPlugin *plugin;
  uint8_t page;  // tile column: 0 = dash, 1 = race page
};

// kPluginMenu must stay last: its pull-down panel is created after every
// other plugin's objects so it renders on top of them. Its drag zone, scrim
// and panel live inside tile 0, so the menu is only reachable from the dash
// page — accepted for now.
static const PluginSlot kSlots[] = {
    {&kPluginTrendRing, 0}, {&kPluginDelta, 0},  {&kPluginLapTime, 0},
    {&kPluginSpeed, 0},     {&kPluginLaps, 0},   {&kPluginStatus, 0},
    {&kPluginBattery, 0},   {&kPluginLock, 0},   {&kPluginRacePage, 1},
    {&kPluginMenu, 0},
};

void uiCreate() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

  // Tileview: two full-screen tiles side by side, horizontal swipe snaps
  // between them. Everything is fully transparent (no bg) so the screen's
  // background color — which plugin_delta sets on lv_scr_act() for the
  // delta sign — shows through on BOTH pages.
  lv_obj_t *tv = lv_tileview_create(screen);
  lv_obj_set_size(tv, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *tiles[2];
  tiles[0] = lv_tileview_add_tile(tv, 0, 0, LV_DIR_HOR);  // dash
  tiles[1] = lv_tileview_add_tile(tv, 1, 0, LV_DIR_HOR);  // race page
  for (lv_obj_t *tile : tiles) lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);

  // Tiles are full-screen, so plugins' LV_ALIGN_CENTER offsets land exactly
  // where they did when the screen itself was the parent.
  for (const PluginSlot &s : kSlots) s.plugin->create(tiles[s.page]);
}

void uiTick() {
  DashModel model;
  dataHubGet(model);
  // Both pages update every tick regardless of which tile is visible; the
  // off-screen page's work is cheap because every plugin gates its LVGL
  // writes behind a changed-value cache.
  for (const PluginSlot &s : kSlots) s.plugin->update(model);
}
