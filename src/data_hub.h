// Data hub: FreeRTOS task (core 0) that aggregates the BLE snapshot and PMIC
// readings into a DashModel and computes derived values (delta trend).
// The render task pulls the latest model with dataHubGet().
#pragma once

#include "dash_model.h"

// Starts the aggregation task. Call after rcMonitorStart()/powerMonStart().
void dataHubStart();

// Thread-safe copy of the latest model.
void dataHubGet(DashModel &out);
