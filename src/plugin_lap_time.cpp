// Current lap time plugin: big readout, equal billing with the delta.
// Uses the 72pt digits-only font (covers 0-9 : . + -), line_height 73.
#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

LV_FONT_DECLARE(font_montserrat_num_72);

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &font_montserrat_num_72, 0);
  lv_obj_set_style_text_color(sLabel, COL_TEXT, 0);
  lv_label_set_text(sLabel, "-:--.-");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, 12);
}

static void update(const DashModel &m) {
  char buf[16];
  numFmtLapTime(buf, sizeof(buf), m.values[RC_CH_LAP_TIME]);
  uiSetLabelText(sLabel, buf);
}

extern const UiPlugin kPluginLapTime = {"lap_time", create, update};
