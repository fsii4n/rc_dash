// The dashboard's data model: everything the UI renders, in one struct.
//
// Produced by the data hub task (core 0), consumed by the render task
// (core 1). UI plugins only ever see this struct — they know nothing about
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

struct DashModel {
  int32_t values[RC_CH_COUNT];  // raw channel values, RC_INVALID_VALUE if unknown
  DeltaTrend trend;
  RcState state;
  PowerStatus power;
};
