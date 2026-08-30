// Outer frame plugin: rounded-rect border hugging the screen edge, colored
// by the delta trend — green while gaining time on the comparison lap, red
// while losing. (The 410x502 panel is rectangular, so the old bezel arc
// became a border frame.)
#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sRing;

static void create(lv_obj_t *screen) {
  sRing = lv_obj_create(screen);
  lv_obj_remove_style_all(sRing);
  lv_obj_set_size(sRing, 410, 502);
  lv_obj_center(sRing);
  lv_obj_set_style_radius(sRing, 48, 0);
  lv_obj_set_style_border_width(sRing, 14, 0);
  lv_obj_set_style_border_color(sRing, colorTrendRing(TREND_INVALID), 0);
  lv_obj_clear_flag(sRing, LV_OBJ_FLAG_CLICKABLE);
}

static void update(const DashModel &m) {
  static DeltaTrend lastTrend = TREND_INVALID;
  if (m.trend == lastTrend) return;
  lastTrend = m.trend;
  lv_obj_set_style_border_color(sRing, colorTrendRing(m.trend), 0);
}

extern const UiPlugin kPluginTrendRing = {"trend_ring", create, update};
