// RaceChrono DIY "monitor" BLE peripheral.
//
// Protocol (see https://github.com/aollin/racechrono-ble-diy-device):
// the device exposes service 0x1ff8 with
//   - characteristic 0x05 (read|write|indicate): device indicates ADD commands
//     carrying channel equations; RaceChrono writes back command results
//   - characteristic 0x06 (write without response): RaceChrono streams packed
//     {uint8 monitorId, int32 big-endian value} tuples
#pragma once

#include <stdint.h>

// Monitor slots, in the order they are registered with RaceChrono.
enum RcChannel : uint8_t {
  RC_CH_SPEED = 0,      // GPS speed, m/s * 10
  RC_CH_LAP_NUMBER,     // current lap number
  RC_CH_LAP_TIME,       // current lap time, s * 10
  RC_CH_PREV_LAP_TIME,  // previous lap time, s * 10
  RC_CH_BEST_LAP_TIME,  // best lap time, s * 10
  RC_CH_DELTA,          // live delta vs comparison lap, s * 100 (signed)
  RC_CH_COUNT
};

static const int32_t RC_INVALID_VALUE = 0x7fffffff;

enum RcState : uint8_t {
  RC_STATE_ADVERTISING = 0,  // waiting for RaceChrono to connect
  RC_STATE_CONFIGURING,      // connected, registering monitors
  RC_STATE_STREAMING,        // monitors accepted, values flowing
};

struct RcSnapshot {
  int32_t values[RC_CH_COUNT];
  RcState state;
  uint32_t lastDataMs;  // millis() of the last value write, 0 if none
};

// Starts NimBLE advertising and the FreeRTOS task that drives the
// configuration handshake. Call once from setup().
void rcMonitorStart();

// Thread-safe copy of the latest values + connection state.
void rcMonitorGet(RcSnapshot &out);
