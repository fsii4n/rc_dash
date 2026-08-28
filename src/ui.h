#pragma once

// Builds the LVGL screen from the plugin registry. Call once after
// lv_init() + display registration.
void uiCreate();

// Pulls the latest DashModel from the data hub and updates every plugin.
// Call from the render (LVGL) task, ~10 Hz.
void uiTick();
