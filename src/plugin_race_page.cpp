// Race page plugin: the entire second tile of the tileview — a stripped-down
// "glance" view showing only the three delta signals: the live-delta sign
// (screen background color, owned globally by plugin_delta and visible here
// because the tileview/tiles are transparent), the delta trend (a thick
// full ring hugging the bezel, same color mapping as plugin_trend_ring),
// and the delta time itself in a huge 110px digits-only font
// (src/font_montserrat_num_110.c, glyphs U+002B..U+003A only — no
// transform_zoom, see docs/DEVELOPMENT.md).
#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

LV_FONT_DECLARE(font_montserrat_num_110);

static lv_obj_t *sRing;
static lv_obj_t *sLabel;

// `screen` is the race tile (tile 1,0), full-screen sized, so the same
// centered alignment used on the dash tile applies unchanged.
static void create(lv_obj_t *screen) {
  // Delta trend: a chunkier version of plugin_trend_ring's bezel ring.
  sRing = lv_arc_create(screen);
  lv_obj_set_size(sRing, 460, 460);
  lv_obj_center(sRing);
  lv_arc_set_bg_angles(sRing, 0, 360);
  lv_obj_remove_style(sRing, NULL, LV_PART_KNOB);
  lv_obj_remove_style(sRing, NULL, LV_PART_INDICATOR);
  lv_obj_clear_flag(sRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(sRing, 36, LV_PART_MAIN);
  lv_obj_set_style_arc_color(sRing, colorTrendRing(TREND_INVALID),
                             LV_PART_MAIN);

  // The delta number, as huge as the dial allows: "+0.00" at 110px is
  // ~330px wide, inside the 466px dial width at the vertical center.
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &font_montserrat_num_110, 0);
  lv_obj_set_style_text_color(sLabel, COL_DIM, 0);
  lv_label_set_text(sLabel, "-.--");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, 0);
}

static void update(const DashModel &m) {
  // Ring color: only on trend change (cache like plugin_trend_ring).
  static DeltaTrend lastTrend = TREND_INVALID;
  if (m.trend != lastTrend) {
    lastTrend = m.trend;
    lv_obj_set_style_arc_color(sRing, colorTrendRing(m.trend), LV_PART_MAIN);
  }

  // Delta number: white while valid, dim placeholder otherwise. The screen
  // background (delta sign color) is NOT set here — plugin_delta owns it,
  // and it is global to both tiles.
  int32_t delta = m.values[RC_CH_DELTA];
  static int32_t lastDelta = RC_INVALID_VALUE - 1;
  if (delta == lastDelta) return;

  char buf[16];
  numFmtDeltaSec(buf, sizeof(buf), delta);
  uiSetLabelText(sLabel, buf);

  bool wasValid = lastDelta != RC_INVALID_VALUE;
  bool isValid = delta != RC_INVALID_VALUE;
  if (lastDelta == RC_INVALID_VALUE - 1 || wasValid != isValid) {
    lv_obj_set_style_text_color(sLabel, colorDeltaText(delta), 0);
  }
  lastDelta = delta;
}

extern const UiPlugin kPluginRacePage = {"race_page", create, update};
