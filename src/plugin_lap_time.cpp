// Current lap time plugin: big readout, equal billing with the delta.
#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sLabel, COL_TEXT, 0);
  lv_label_set_text(sLabel, "-:--.-");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, 48);
}

static void update(const DashModel &m) {
  char buf[16];
  numFmtLapTime(buf, sizeof(buf), m.values[RC_CH_LAP_TIME]);
  uiSetLabelText(sLabel, buf);
}

extern const UiPlugin kPluginLapTime = {"lap_time", create, update};
