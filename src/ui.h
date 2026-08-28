#pragma once

#include "rc_monitor.h"

// Builds the LVGL screen. Call once after lv_init() + display registration.
void uiCreate();

// Pushes a fresh RaceChrono snapshot into the widgets. Call from the LVGL task.
void uiUpdate(const RcSnapshot &snap);
