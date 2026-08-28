#include "touch_input.h"

#include <Arduino.h>
#include <TouchDrv.hpp>
#include <Wire.h>
#include <lvgl.h>

#include "data_hub.h"
#include "i2c_bus.h"
#include "pin_config.h"

static TouchDrvCST92xx sTouch;
static bool sTouchOk = false;

// Runs inside lv_timer_handler on the LVGL task (core 1) every
// LV_INDEV_DEF_READ_PERIOD ms. Each poll is one short I2C read of the touch
// registers, taken under the shared bus mutex so it can't interleave with
// the AXP2101 poll on core 0.
static void touchReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static lv_coord_t sLastX = 0, sLastY = 0;

  data->state = LV_INDEV_STATE_RELEASED;
  data->point.x = sLastX;
  data->point.y = sLastY;
  if (!sTouchOk) return;

  i2cBusLock();
  const TouchPoints &pts = sTouch.getTouchPoints();
  i2cBusUnlock();
  // pts references driver-internal state only ever mutated from this task,
  // so reading it after unlock is safe — the mutex protects the Wire bus.

  if (!pts.hasPoints()) return;

  // Touch lock gate: report RELEASED and swallow the point entirely.
  if (dataHubGetTouchLocked()) return;

  const TouchPoint &p = pts.getPoint(0);
  sLastX = (lv_coord_t)(p.x < LCD_WIDTH ? p.x : LCD_WIDTH - 1);
  sLastY = (lv_coord_t)(p.y < LCD_HEIGHT ? p.y : LCD_HEIGHT - 1);
  data->point.x = sLastX;
  data->point.y = sLastY;
  data->state = LV_INDEV_STATE_PRESSED;
}

bool touchInputInit() {
  // The power_mon task is already polling the PMIC on this bus, so even the
  // driver's begin() (reset sequence + firmware probe) must hold the mutex.
  sTouch.setPins(TP_RST, TP_INT);
  sTouch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  i2cBusLock();
  sTouchOk = sTouch.begin(Wire, CST92XX_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  i2cBusUnlock();
  if (!sTouchOk) {
    Serial.println("[touch] CST9217 not found on I2C!");
    return false;
  }
  Serial.printf("[touch] %s online\n", sTouch.getModelName());

  static lv_indev_drv_t sIndevDrv;
  lv_indev_drv_init(&sIndevDrv);
  sIndevDrv.type = LV_INDEV_TYPE_POINTER;
  sIndevDrv.read_cb = touchReadCb;
  lv_indev_drv_register(&sIndevDrv);
  return true;
}
