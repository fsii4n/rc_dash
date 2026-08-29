#include "data_hub.h"

#include <Arduino.h>
#include <string.h>

#include "pin_config.h"

static portMUX_TYPE sLock = portMUX_INITIALIZER_UNLOCKED;
static DashModel sModel;
static volatile bool sTouchLocked = false;

void dataHubSetTouchLocked(bool locked) { sTouchLocked = locked; }
bool dataHubGetTouchLocked() { return sTouchLocked; }

// ---- touch-lock button (BOOT key, GPIO0, active low) -----------------------

// Debounced by the 50 ms task cadence: the raw level must read the same on
// two consecutive polls (50-100 ms stable) before the state changes, and the
// lock toggles on the press edge only. No ISR needed for a human press.
static bool sBtnStable = false;   // debounced "pressed" state
static bool sBtnLastRaw = false;  // previous raw sample

static void pollLockButton() {
  bool raw = digitalRead(KEY_BOOT) == LOW;
  if (raw == sBtnLastRaw && raw != sBtnStable) {
    sBtnStable = raw;
    if (sBtnStable) {  // press edge
      dataHubSetTouchLocked(!dataHubGetTouchLocked());
      Serial.printf("[hub] touch %s\n", sTouchLocked ? "locked" : "unlocked");
    }
  }
  sBtnLastRaw = raw;
}

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

// ---- session stats (lap history + max speed) -------------------------------

static LapRecord sHistory[DASH_LAP_HISTORY];
static uint8_t sHistoryCount = 0;
static int32_t sMaxSpeed = RC_INVALID_VALUE;
static int32_t sLastLapNumber = RC_INVALID_VALUE;

static void updateSession(const RcSnapshot &snap) {
  int32_t lap = snap.values[RC_CH_LAP_NUMBER];
  int32_t prev = snap.values[RC_CH_PREV_LAP_TIME];
  int32_t speed = snap.values[RC_CH_SPEED];

  if (speed != RC_INVALID_VALUE &&
      (sMaxSpeed == RC_INVALID_VALUE || speed > sMaxSpeed)) {
    sMaxSpeed = speed;
  }
  if (lap == RC_INVALID_VALUE) return;

  if (sLastLapNumber != RC_INVALID_VALUE && lap < sLastLapNumber) {
    // Lap counter went backwards: new session started in RaceChrono.
    sHistoryCount = 0;
    sMaxSpeed = RC_INVALID_VALUE;
  }
  // On a lap increment the previous_lap_time channel carries the time of the
  // lap that just finished (both arrive in the same 10 Hz packet).
  if (sLastLapNumber != RC_INVALID_VALUE && lap > sLastLapNumber &&
      prev != RC_INVALID_VALUE) {
    memmove(&sHistory[1], &sHistory[0],
            sizeof(LapRecord) * (DASH_LAP_HISTORY - 1));
    sHistory[0].lapNumber = sLastLapNumber;
    sHistory[0].timeDeci = prev;
    if (sHistoryCount < DASH_LAP_HISTORY) sHistoryCount++;
  }
  sLastLapNumber = lap;
}

// ---- aggregation task ------------------------------------------------------

static void dataTask(void *arg) {
  for (;;) {
    pollLockButton();

    RcSnapshot snap;
    rcMonitorGet(snap);
    PowerStatus power;
    powerMonGet(power);
    DeltaTrend trend = computeTrend(snap.values[RC_CH_DELTA], millis());
    updateSession(snap);

    portENTER_CRITICAL(&sLock);
    memcpy(sModel.values, snap.values, sizeof(sModel.values));
    sModel.trend = trend;
    sModel.state = snap.state;
    sModel.power = power;
    sModel.touchLocked = sTouchLocked;
    memcpy(sModel.history, sHistory, sizeof(sModel.history));
    sModel.historyCount = sHistoryCount;
    sModel.maxSpeedRaw = sMaxSpeed;
    portEXIT_CRITICAL(&sLock);

    static uint32_t sLastStackReport = 0;
    uint32_t now = millis();
    if (now - sLastStackReport >= 60000) {
      sLastStackReport = now;
      Serial.printf("[hub] stack free %u\n",
                    (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void dataHubStart() {
  // External 10K pull-up on board; the internal one is belt-and-braces.
  pinMode(KEY_BOOT, INPUT_PULLUP);

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
