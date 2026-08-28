// Color rendering module: maps data values to colors. All color decisions
// (delta sign, delta trend, battery state) live here so plugins share one
// palette and one set of rules.
#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "dash_model.h"

// Shared palette
extern const lv_color_t COL_TEXT;  // primary text
extern const lv_color_t COL_DIM;   // secondary text / placeholders
extern const lv_color_t COL_GOOD;  // green accents (best lap, charging)
extern const lv_color_t COL_WARN;  // amber
extern const lv_color_t COL_BAD;   // red accents

// Live delta sign (green = faster than comparison, red = slower).
lv_color_t colorDeltaBg(int32_t deltaCenti);    // full-screen background
lv_color_t colorDeltaText(int32_t deltaCenti);  // headline number

// Delta trend (green = gaining time, red = losing time).
lv_color_t colorTrendRing(DeltaTrend trend);

// Battery indicator by charge state / percent.
lv_color_t colorBattery(const PowerStatus &power);
