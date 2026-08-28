#include "data_hub.h"

#include <Arduino.h>
#include <string.h>

static portMUX_TYPE sLock = portMUX_INITIALIZER_UNLOCKED;
static DashModel sModel;
static volatile bool sTouchLocked = false;

void dataHubSetTouchLocked(bool locked) { sTouchLocked = locked; }
bool dataHubGetTouchLocked() { return sTouchLocked; }

// ---- delta trend (derivative of the live delta) ----------------------------

// EMA-smoothed rate in centiseconds/second; below the threshold reads FLAT.
static const float kTrendThreshold = 4.0f;
static const float kRateClamp = 100.0f;  // ignore lap-crossing delta jumps

static int32_t sPrevDelta = RC_INVALID_VALUE;
static uint32_t sPrevMs = 0;
static float sRate = 0.0f;

static DeltaTrend computeTrend(int32_t delta, uint32_t nowMs) {
  if (delta == RC_INVALID_VALUE) {
    sPrevDelta = RC_INVALID_VALUE;
    sRate = 0.0f;
    return TREND_INVALID;
  }
  if (sPrevDelta == RC_INVALID_VALUE) {
    sPrevDelta = delta;
    sPrevMs = nowMs;
    sRate = 0.0f;
    return TREND_FLAT;
  }
  if (delta != sPrevDelta) {
    float dt = (nowMs - sPrevMs) / 1000.0f;
    if (dt > 0.02f) {
      float inst = (delta - sPrevDelta) / dt;
      if (inst > -kRateClamp && inst < kRateClamp) {
        sRate = 0.7f * sRate + 0.3f * inst;
      }
    }
    sPrevDelta = delta;
    sPrevMs = nowMs;
  }
  if (sRate <= -kTrendThreshold) return TREND_IMPROVING;
  if (sRate >= kTrendThreshold) return TREND_LOSING;
  return TREND_FLAT;
}

// ---- aggregation task ------------------------------------------------------

static void dataTask(void *arg) {
  for (;;) {
    RcSnapshot snap;
    rcMonitorGet(snap);
    PowerStatus power;
    powerMonGet(power);
    DeltaTrend trend = computeTrend(snap.values[RC_CH_DELTA], millis());

    portENTER_CRITICAL(&sLock);
    memcpy(sModel.values, snap.values, sizeof(sModel.values));
    sModel.trend = trend;
    sModel.state = snap.state;
    sModel.power = power;
    sModel.touchLocked = sTouchLocked;
    portEXIT_CRITICAL(&sLock);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void dataHubStart() {
  for (int i = 0; i < RC_CH_COUNT; i++) sModel.values[i] = RC_INVALID_VALUE;
  sModel.trend = TREND_INVALID;
  sModel.state = RC_STATE_ADVERTISING;
  memset(&sModel.power, 0, sizeof(sModel.power));
  xTaskCreatePinnedToCore(dataTask, "datahub", 4096, nullptr, 2, nullptr, 0);
}

void dataHubGet(DashModel &out) {
  portENTER_CRITICAL(&sLock);
  out = sModel;
  portEXIT_CRITICAL(&sLock);
}
