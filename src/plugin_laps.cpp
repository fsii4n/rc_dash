// Prev/best lap row plugin: last lap time and session best.
#include <stdio.h>

#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sPrev;
static lv_obj_t *sBest;

static void create(lv_obj_t *screen) {
  sPrev = lv_label_create(screen);
  lv_obj_set_style_text_font(sPrev, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sPrev, COL_DIM, 0);
  lv_label_set_text(sPrev, "PREV -:--.-");
  lv_obj_align(sPrev, LV_ALIGN_CENTER, -80, 112);

  sBest = lv_label_create(screen);
  lv_obj_set_style_text_font(sBest, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sBest, COL_GOOD, 0);
  lv_label_set_text(sBest, "BEST -:--.-");
  lv_obj_align(sBest, LV_ALIGN_CENTER, 80, 112);
}

static void update(const DashModel &m) {
  char t[16], buf[24];
  numFmtLapTime(t, sizeof(t), m.values[RC_CH_PREV_LAP_TIME]);
  snprintf(buf, sizeof(buf), "PREV %s", t);
  uiSetLabelText(sPrev, buf);

  numFmtLapTime(t, sizeof(t), m.values[RC_CH_BEST_LAP_TIME]);
  snprintf(buf, sizeof(buf), "BEST %s", t);
  uiSetLabelText(sBest, buf);
}

extern const UiPlugin kPluginLaps = {"laps", create, update};
