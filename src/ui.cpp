// Screen composition: the plugin registry. Each element of the 466x466
// round AMOLED is a self-contained UiPlugin; this file only decides which
// plugins exist and the order they update in (delta owns the screen
// background, so it runs early).
//
// Kart-focused layout, top to bottom:
//   - outer ring ....... delta trend (green = gaining, red = losing)
//   - battery .......... top of the dial
//   - speed ............ small km/h readout
//   - live delta ....... huge, screen bg green/red by its sign
//   - current lap time . equal billing with the delta
//   - prev/best row
//   - status ........... lap number / connection state
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
extern const UiPlugin kPluginMenu;

// kPluginMenu must stay last: its pull-down panel is created after every
// other plugin's objects so it renders on top of them.
static const UiPlugin *const kPlugins[] = {
    &kPluginTrendRing, &kPluginDelta,  &kPluginLapTime, &kPluginSpeed,
    &kPluginLaps,      &kPluginStatus, &kPluginBattery, &kPluginMenu,
};

void uiCreate() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  for (const UiPlugin *p : kPlugins) p->create(screen);
}

void uiTick() {
  DashModel model;
  dataHubGet(model);
  for (const UiPlugin *p : kPlugins) p->update(model);
}
