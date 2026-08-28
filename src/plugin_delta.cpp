// Live delta plugin: the headline number (seconds vs the comparison lap).
// Also owns the full-screen background color: green when faster than the
// comparison lap, red when slower, black while no comparison lap exists.
//
// Plain 48pt label on purpose: transform_zoom'd labels render through LVGL
// draw layers, which silently draw nothing when the layer buffer can't be
// allocated and cost 60ms+ per repaint when it can. The color field is the
// glanceable signal; the number doesn't need to be bigger than the lap time.
#include <stdlib.h>

#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sLabel, COL_DIM, 0);
  lv_label_set_text(sLabel, "-.--");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, -42);
}

static void update(const DashModel &m) {
  int32_t delta = m.values[RC_CH_DELTA];
  static int32_t lastDelta = RC_INVALID_VALUE - 1;
  if (delta == lastDelta) return;
  lastDelta = delta;

  char buf[16];
  numFmtDeltaSec(buf, sizeof(buf), delta);
  uiSetLabelText(sLabel, buf);

  // Background/text color by validity + sign, with hysteresis: inside
  // ±0.10s keep the previous color, so a delta wobbling around zero doesn't
  // trigger a full-screen repaint (~65ms) every tick.
  static int shown = -1;  // 0 = faster, 1 = slower, 2 = invalid
  int want;
  if (delta == RC_INVALID_VALUE) {
    want = 2;
  } else if ((shown == 0 || shown == 1) && labs(delta) < 10) {
    want = shown;
  } else {
    want = delta <= 0 ? 0 : 1;
  }
  if (want != shown) {
    shown = want;
    int32_t proxy = want == 2 ? RC_INVALID_VALUE : (want == 0 ? -1 : 1);
    lv_obj_set_style_text_color(sLabel, colorDeltaText(proxy), 0);
    lv_obj_set_style_bg_color(lv_scr_act(), colorDeltaBg(proxy), 0);
  }
}

extern const UiPlugin kPluginDelta = {"delta", create, update};
