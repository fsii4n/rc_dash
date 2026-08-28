// CST9217 touch controller (SensorLib TouchDrvCST92xx) + LVGL pointer indev.
//
// Call touchInputInit() from setup() after lv_init() and display driver
// registration (the indev attaches to the default display) and BEFORE the
// LVGL render task is created, so the lv_indev registration isn't concurrent
// with lv_timer_handler. All I2C traffic goes through the shared bus mutex
// (i2c_bus.h) because power_mon's task polls the AXP2101 on the same bus.
//
// While dataHubGetTouchLocked() is true the indev reports RELEASED and
// swallows all touches.
#pragma once

// Returns true if the CST9217 was found and the LVGL indev registered.
bool touchInputInit();
