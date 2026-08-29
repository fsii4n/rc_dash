// Lap history page: the last DASH_LAP_HISTORY completed laps, most recent
// on top, session best highlighted green; session stats at the bottom.
// Live equivalent of the post-session lap recall on MyChron/Alfano timers.
#include <stdio.h>

#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

static lv_obj_t *sTitle;
static lv_obj_t *sRows[DASH_LAP_HISTORY];
static lv_obj_t *sStats;

static void create(lv_obj_t *screen) {
  sTitle = lv_label_create(screen);
  lv_obj_set_style_text_font(sTitle, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sTitle, COL_DIM, 0);
  lv_label_set_text(sTitle, "LAPS");
  lv_obj_align(sTitle, LV_ALIGN_CENTER, 0, -160);

  for (int i = 0; i < DASH_LAP_HISTORY; i++) {
    sRows[i] = lv_label_create(screen);
    lv_obj_set_style_text_font(sRows[i], &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(sRows[i], COL_TEXT, 0);
    lv_label_set_text(sRows[i], "");
    lv_obj_align(sRows[i], LV_ALIGN_CENTER, 0, -115 + i * 45);
  }

  sStats = lv_label_create(screen);
  lv_obj_set_style_text_font(sStats, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sStats, COL_DIM, 0);
  lv_label_set_text(sStats, "");
  lv_obj_align(sStats, LV_ALIGN_CENTER, 0, 168);
}

static void update(const DashModel &m) {
  int32_t best = m.values[RC_CH_BEST_LAP_TIME];

  char t[16], buf[32];
  for (int i = 0; i < DASH_LAP_HISTORY; i++) {
    if (i >= m.historyCount) {
      uiSetLabelText(sRows[i], i == 0 && m.historyCount == 0 ? "-" : "");
      continue;
    }
    const LapRecord &r = m.history[i];
    numFmtLapTime(t, sizeof(t), r.timeDeci);
    snprintf(buf, sizeof(buf), "L%ld  %s", (long)r.lapNumber, t);
    uiSetLabelText(sRows[i], buf);
    // Highlight the session best; colors only change when rows shift.
    static uint8_t lastBestRow[DASH_LAP_HISTORY] = {0};
    uint8_t isBest = (best != RC_INVALID_VALUE && r.timeDeci == best) ? 1 : 0;
    if (isBest != lastBestRow[i]) {
      lastBestRow[i] = isBest;
      lv_obj_set_style_text_color(sRows[i], isBest ? COL_GOOD : COL_TEXT, 0);
    }
  }

  int32_t lap = m.values[RC_CH_LAP_NUMBER];
  char spd[8];
  numFmtSpeedKmh(spd, sizeof(spd), m.maxSpeedRaw);
  if (lap == RC_INVALID_VALUE) {
    snprintf(buf, sizeof(buf), "MAX %s km/h", spd);
  } else {
    snprintf(buf, sizeof(buf), "LAP %ld   MAX %s km/h", (long)lap, spd);
  }
  uiSetLabelText(sStats, buf);
}

extern const UiPlugin kPluginLapHistory = {"lap_history", create, update};
