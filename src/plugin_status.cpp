// Status plugin: connection state / lap number at the bottom of the dash.
#include <stdio.h>

#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(sLabel, COL_DIM, 0);
  lv_label_set_text(sLabel, "WAITING FOR RACECHRONO");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, 156);
}

static void update(const DashModel &m) {
  char buf[24];
  switch (m.state) {
    case RC_STATE_ADVERTISING:
      uiSetLabelText(sLabel, "WAITING FOR RACECHRONO");
      return;
    case RC_STATE_CONFIGURING:
      uiSetLabelText(sLabel, "CONNECTED, CONFIGURING...");
      return;
    case RC_STATE_STREAMING: {
      int32_t lap = m.values[RC_CH_LAP_NUMBER];
      if (lap == RC_INVALID_VALUE) {
        uiSetLabelText(sLabel, "LIVE");
      } else {
        snprintf(buf, sizeof(buf), "LAP %ld", (long)lap);
        uiSetLabelText(sLabel, buf);
      }
      return;
    }
  }
}

extern const UiPlugin kPluginStatus = {"status", create, update};
