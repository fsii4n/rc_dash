// The dashboard's data model: everything the UI renders, in one struct.
//
// Produced by the data hub task, consumed by the render task.
// UI plugins only ever see this struct — they know nothing about
// BLE, the PMIC, or how derived values are computed.
#pragma once

#include <stdint.h>

#include "power_mon.h"
#include "rc_monitor.h"

// Direction the live delta is moving in (its derivative), independent of
// the delta's sign: you can be ahead of the comparison lap but losing time.
enum DeltaTrend : uint8_t {
  TREND_INVALID = 0,  // no live delta (no comparison lap yet)
  TREND_FLAT,         // holding steady
  TREND_IMPROVING,    // delta shrinking: gaining time
  TREND_LOSING,       // delta growing: losing time
};

// Completed laps kept for the lap-history page, most recent first.
#define DASH_LAP_HISTORY 6

struct LapRecord {
  int32_t lapNumber;
  int32_t timeDeci;  // s * 10
};

struct DashModel {
  int32_t values[RC_CH_COUNT];  // raw channel values, RC_INVALID_VALUE if unknown
  DeltaTrend trend;
  RcState state;
  PowerStatus power;
  bool touchLocked;  // physical lock button: UI ignores touch while set
  LapRecord history[DASH_LAP_HISTORY];
  uint8_t historyCount;
  int32_t maxSpeedRaw;  // session max, m/s * 10, RC_INVALID_VALUE if none
};
