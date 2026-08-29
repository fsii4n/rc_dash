// Predictive lap time page: predicted final time for the current lap
// (best lap + live delta — RaceChrono's comparison lap is the session best),
// green while on course to beat the best, red otherwise. Same idea as the
// MyChron "theoretical" recall and RaceChrono's Predictive Lap Timer, but
// live on its own page.
#include <stdio.h>
#include <stdlib.h>

#include "render_color.h"
#include "render_num.h"
#include "ui_plugin.h"

LV_FONT_DECLARE(font_montserrat_num_72);

static lv_obj_t *sCaption;
static lv_obj_t *sPredicted;
static lv_obj_t *sBest;

static void create(lv_obj_t *screen) {
  sCaption = lv_label_create(screen);
  lv_obj_set_style_text_font(sCaption, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sCaption, COL_DIM, 0);
  lv_label_set_text(sCaption, "PREDICTED");
  lv_obj_align(sCaption, LV_ALIGN_CENTER, 0, -110);

  sPredicted = lv_label_create(screen);
  lv_obj_set_style_text_font(sPredicted, &font_montserrat_num_72, 0);
  lv_obj_set_style_text_color(sPredicted, COL_DIM, 0);
  lv_label_set_text(sPredicted, "-:--.-");
  lv_obj_align(sPredicted, LV_ALIGN_CENTER, 0, -25);

  sBest = lv_label_create(screen);
  lv_obj_set_style_text_font(sBest, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(sBest, COL_GOOD, 0);
  lv_label_set_text(sBest, "BEST -:--.-");
  lv_obj_align(sBest, LV_ALIGN_CENTER, 0, 70);
}

static void update(const DashModel &m) {
  int32_t best = m.values[RC_CH_BEST_LAP_TIME];
  int32_t delta = m.values[RC_CH_DELTA];

  char t[16], buf[24];
  numFmtLapTime(t, sizeof(t), best);
  snprintf(buf, sizeof(buf), "BEST %s", t);
  uiSetLabelText(sBest, buf);

  static int32_t lastPredDeci = RC_INVALID_VALUE - 1;
  static int lastSign = -1;
  int32_t predDeci;
  int sign;
  if (best == RC_INVALID_VALUE || delta == RC_INVALID_VALUE) {
    predDeci = RC_INVALID_VALUE;
    sign = 2;
  } else {
    // delta is centiseconds; round to the deciseconds the display uses
    predDeci = best + (delta >= 0 ? (delta + 5) / 10 : (delta - 5) / 10);
    sign = delta <= 0 ? 0 : 1;
  }
  if (predDeci == lastPredDeci && sign == lastSign) return;
  lastPredDeci = predDeci;

  numFmtLapTime(t, sizeof(t), predDeci);
  uiSetLabelText(sPredicted, t);
  if (sign != lastSign) {
    lastSign = sign;
    lv_obj_set_style_text_color(
        sPredicted,
        sign == 2 ? COL_DIM : (sign == 0 ? COL_GOOD : COL_BAD), 0);
  }
}

extern const UiPlugin kPluginPredictive = {"predictive", create, update};
