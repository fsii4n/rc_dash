// Outer ring plugin: full circle hugging the bezel, colored by the delta
// trend — green while gaining time on the comparison lap, red while losing.
#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sRing;

static void create(lv_obj_t *screen) {
  sRing = lv_arc_create(screen);
  lv_obj_set_size(sRing, 456, 456);
  lv_obj_center(sRing);
  lv_arc_set_bg_angles(sRing, 0, 360);
  lv_obj_remove_style(sRing, NULL, LV_PART_KNOB);
  lv_obj_remove_style(sRing, NULL, LV_PART_INDICATOR);
  lv_obj_clear_flag(sRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(sRing, 20, LV_PART_MAIN);
  lv_obj_set_style_arc_color(sRing, colorTrendRing(TREND_INVALID), LV_PART_MAIN);
}

static void update(const DashModel &m) {
  static DeltaTrend lastTrend = TREND_INVALID;
  if (m.trend == lastTrend) return;
  lastTrend = m.trend;
  lv_obj_set_style_arc_color(sRing, colorTrendRing(m.trend), LV_PART_MAIN);
}

extern const UiPlugin kPluginTrendRing = {"trend_ring", create, update};
