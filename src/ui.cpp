#include "ui.h"

#include <lvgl.h>
#include <stdio.h>

// 466x466 round AMOLED. Layout:
//   - outer arc: speed gauge 0..280 km/h
//   - center: big speed readout + km/h
//   - below: current lap time, then prev/best row
//   - top: lap number / connection status

#define SPEED_ARC_MAX_KMH 280

static lv_obj_t *sArc;
static lv_obj_t *sSpeedLabel;
static lv_obj_t *sLapTimeLabel;
static lv_obj_t *sPrevLabel;
static lv_obj_t *sBestLabel;
static lv_obj_t *sStatusLabel;

static const lv_color_t COL_ACCENT = lv_color_make(0xff, 0x5a, 0x00);
static const lv_color_t COL_DIM = lv_color_make(0x50, 0x50, 0x50);
static const lv_color_t COL_TEXT = lv_color_make(0xf0, 0xf0, 0xf0);

// Formats deci-seconds as "m:ss.t" (or "--:--.-" when invalid).
static void formatLapTime(char *buf, size_t n, int32_t deciSeconds) {
  if (deciSeconds == RC_INVALID_VALUE || deciSeconds < 0) {
    snprintf(buf, n, "-:--.-");
    return;
  }
  int32_t tenths = deciSeconds % 10;
  int32_t total = deciSeconds / 10;
  snprintf(buf, n, "%ld:%02ld.%ld", (long)(total / 60), (long)(total % 60),
           (long)tenths);
}

void uiCreate() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Speed arc hugging the round bezel
  sArc = lv_arc_create(scr);
  lv_obj_set_size(sArc, 440, 440);
  lv_obj_center(sArc);
  lv_arc_set_rotation(sArc, 120);
  lv_arc_set_bg_angles(sArc, 0, 300);
  lv_arc_set_range(sArc, 0, SPEED_ARC_MAX_KMH);
  lv_arc_set_value(sArc, 0);
  lv_obj_remove_style(sArc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(sArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(sArc, 14, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sArc, 14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sArc, lv_color_make(0x28, 0x28, 0x28), LV_PART_MAIN);
  lv_obj_set_style_arc_color(sArc, COL_ACCENT, LV_PART_INDICATOR);

  // Big speed readout
  sSpeedLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(sSpeedLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sSpeedLabel, COL_TEXT, 0);
  lv_label_set_text(sSpeedLabel, "0");
  lv_obj_align(sSpeedLabel, LV_ALIGN_CENTER, 0, -60);

  lv_obj_t *unit = lv_label_create(scr);
  lv_obj_set_style_text_font(unit, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(unit, COL_DIM, 0);
  lv_label_set_text(unit, "km/h");
  lv_obj_align(unit, LV_ALIGN_CENTER, 0, -20);

  // Current lap time
  sLapTimeLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(sLapTimeLabel, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(sLapTimeLabel, COL_TEXT, 0);
  lv_label_set_text(sLapTimeLabel, "-:--.-");
  lv_obj_align(sLapTimeLabel, LV_ALIGN_CENTER, 0, 40);

  // Previous / best lap row
  sPrevLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(sPrevLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sPrevLabel, COL_DIM, 0);
  lv_label_set_text(sPrevLabel, "PREV -:--.-");
  lv_obj_align(sPrevLabel, LV_ALIGN_CENTER, -80, 95);

  sBestLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(sBestLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sBestLabel, lv_color_make(0x30, 0xc0, 0x50), 0);
  lv_label_set_text(sBestLabel, "BEST -:--.-");
  lv_obj_align(sBestLabel, LV_ALIGN_CENTER, 80, 95);

  // Lap number / connection status
  sStatusLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(sStatusLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sStatusLabel, COL_DIM, 0);
  lv_label_set_text(sStatusLabel, "WAITING FOR RACECHRONO");
  lv_obj_align(sStatusLabel, LV_ALIGN_CENTER, 0, 150);
}

void uiUpdate(const RcSnapshot &snap) {
  char buf[32];

  // Speed: channel delivers m/s * 10 -> km/h = v * 0.36
  int32_t rawSpeed = snap.values[RC_CH_SPEED];
  int kmh = (rawSpeed == RC_INVALID_VALUE) ? 0 : (int)(rawSpeed * 0.36f + 0.5f);
  lv_label_set_text_fmt(sSpeedLabel, "%d", kmh);
  lv_arc_set_value(sArc, kmh > SPEED_ARC_MAX_KMH ? SPEED_ARC_MAX_KMH : kmh);

  formatLapTime(buf, sizeof(buf), snap.values[RC_CH_LAP_TIME]);
  lv_label_set_text(sLapTimeLabel, buf);

  char timeBuf[16];
  formatLapTime(timeBuf, sizeof(timeBuf), snap.values[RC_CH_PREV_LAP_TIME]);
  snprintf(buf, sizeof(buf), "PREV %s", timeBuf);
  lv_label_set_text(sPrevLabel, buf);

  formatLapTime(timeBuf, sizeof(timeBuf), snap.values[RC_CH_BEST_LAP_TIME]);
  snprintf(buf, sizeof(buf), "BEST %s", timeBuf);
  lv_label_set_text(sBestLabel, buf);

  switch (snap.state) {
    case RC_STATE_ADVERTISING:
      lv_label_set_text(sStatusLabel, "WAITING FOR RACECHRONO");
      break;
    case RC_STATE_CONFIGURING:
      lv_label_set_text(sStatusLabel, "CONNECTED, CONFIGURING...");
      break;
    case RC_STATE_STREAMING: {
      int32_t lap = snap.values[RC_CH_LAP_NUMBER];
      if (lap == RC_INVALID_VALUE) {
        lv_label_set_text(sStatusLabel, "LIVE");
      } else {
        lv_label_set_text_fmt(sStatusLabel, "LAP %ld", (long)lap);
      }
      break;
    }
  }
}
