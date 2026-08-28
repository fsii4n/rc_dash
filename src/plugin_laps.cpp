// Prev/best lap row plugin: last lap time and session best, side by side.
// No text prefixes — color alone distinguishes them (dim = previous,
// green = best). 48pt, centers at x = -90/+90: a "1:23.4" is ~150px wide
// (half 75), so inner edges sit at +/-15 (30px gap) and outer edges at
// +/-165, inside the ~187px chord halfwidth of the round screen at the
// row's bottom edge (y ~ 139).
#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sPrev;
static lv_obj_t *sBest;

static void create(lv_obj_t *screen) {
  sPrev = lv_label_create(screen);
  lv_obj_set_style_text_font(sPrev, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sPrev, COL_DIM, 0);
  lv_label_set_text(sPrev, "-:--.-");
  lv_obj_align(sPrev, LV_ALIGN_CENTER, -90, 112);

  sBest = lv_label_create(screen);
  lv_obj_set_style_text_font(sBest, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sBest, COL_GOOD, 0);
  lv_label_set_text(sBest, "-:--.-");
  lv_obj_align(sBest, LV_ALIGN_CENTER, 90, 112);
}

static void update(const DashModel &m) {
  char t[16];
  numFmtLapTime(t, sizeof(t), m.values[RC_CH_PREV_LAP_TIME]);
  uiSetLabelText(sPrev, t);

  numFmtLapTime(t, sizeof(t), m.values[RC_CH_BEST_LAP_TIME]);
  uiSetLabelText(sBest, t);
}

extern const UiPlugin kPluginLaps = {"laps", create, update};
