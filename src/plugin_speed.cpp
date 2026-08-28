// Speed plugin: small km/h readout above the delta (kart use — secondary).
#include <stdio.h>

#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(sLabel, COL_TEXT, 0);
  lv_label_set_text(sLabel, "-- km/h");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, -112);
}

static void update(const DashModel &m) {
  char num[8], buf[16];
  numFmtSpeedKmh(num, sizeof(num), m.values[RC_CH_SPEED]);
  snprintf(buf, sizeof(buf), "%s km/h", num);
  uiSetLabelText(sLabel, buf);
}

extern const UiPlugin kPluginSpeed = {"speed", create, update};
