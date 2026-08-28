#include "render_color.h"

const lv_color_t COL_TEXT = LV_COLOR_MAKE(0xf0, 0xf0, 0xf0);
const lv_color_t COL_DIM = LV_COLOR_MAKE(0x90, 0x90, 0x90);
const lv_color_t COL_GOOD = LV_COLOR_MAKE(0x30, 0xc0, 0x50);
const lv_color_t COL_WARN = LV_COLOR_MAKE(0xe0, 0xa0, 0x20);
const lv_color_t COL_BAD = LV_COLOR_MAKE(0xe0, 0x30, 0x30);

// Backgrounds are darker than the ring/text shades so white text stays
// readable and the bright ring still pops against them.
static const lv_color_t kBgFaster = LV_COLOR_MAKE(0x00, 0x7a, 0x24);
static const lv_color_t kBgSlower = LV_COLOR_MAKE(0xb4, 0x10, 0x10);
static const lv_color_t kRingGaining = LV_COLOR_MAKE(0x20, 0xe0, 0x50);
static const lv_color_t kRingLosing = LV_COLOR_MAKE(0xff, 0x30, 0x30);
static const lv_color_t kRingFlat = LV_COLOR_MAKE(0x50, 0x50, 0x50);
static const lv_color_t kRingIdle = LV_COLOR_MAKE(0x28, 0x28, 0x28);

lv_color_t colorDeltaBg(int32_t deltaCenti) {
  if (deltaCenti == RC_INVALID_VALUE) return lv_color_black();
  return deltaCenti <= 0 ? kBgFaster : kBgSlower;
}

lv_color_t colorDeltaText(int32_t deltaCenti) {
  if (deltaCenti == RC_INVALID_VALUE) return COL_DIM;
  return lv_color_white();
}

lv_color_t colorTrendRing(DeltaTrend trend) {
  switch (trend) {
    case TREND_IMPROVING: return kRingGaining;
    case TREND_LOSING: return kRingLosing;
    case TREND_FLAT: return kRingFlat;
    default: return kRingIdle;
  }
}

lv_color_t colorBattery(const PowerStatus &power) {
  if (power.charging) return COL_GOOD;
  if (power.percent <= 15) return COL_BAD;
  if (power.percent <= 30) return COL_WARN;
  return COL_TEXT;
}
