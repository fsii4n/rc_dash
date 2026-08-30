// FT3168 capacitive touch (I2C 0x38) — polled, no interrupt needed.
//
// The FT3x68 register map is the classic FocalTech FT6x36 layout: 0x02 holds
// the touch-point count, 0x03..0x06 the first point's coordinates. That's
// all this dashboard needs (single touch, swipes and taps), so we read the
// registers directly over Wire instead of pulling in a driver library.
#include "touch_input.h"

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

#include "data_hub.h"
#include "i2c_bus.h"
#include "pin_config.h"

static const uint8_t kFtAddr = 0x38;
static const uint8_t kRegTdStatus = 0x02;  // low nibble = touch point count
static const uint8_t kRegChipId = 0xA3;

// If the panel reports coordinates rotated 180° relative to the display
// (like the previous board did), set this to 1 to mirror both axes. The
// [touch] serial log prints raw vs mapped points for on-device checking.
#define TOUCH_ROT_180 0

static bool sTouchOk = false;

// Reads TD_STATUS..P1_YL (0x02..0x06) in one transaction. Returns false on
// bus error or no touch.
static bool ftReadPoint(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(kFtAddr);
  Wire.write(kRegTdStatus);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kFtAddr, (uint8_t)5) != 5) return false;
  uint8_t buf[5];
  for (int i = 0; i < 5; i++) buf[i] = Wire.read();
  uint8_t points = buf[0] & 0x0F;
  if (points == 0 || points > 2) return false;
  x = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
  y = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];
  return true;
}

// Runs inside lv_timer_handler on the LVGL task every
// LV_INDEV_DEF_READ_PERIOD ms. Each poll is one short I2C read of the touch
// registers, taken under the shared bus mutex so it can't interleave with
// the AXP2101 poll from the power task.
static void touchReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static lv_coord_t sLastX = 0, sLastY = 0;
  static bool sWasPressed = false;

  data->state = LV_INDEV_STATE_RELEASED;
  data->point.x = sLastX;
  data->point.y = sLastY;
  if (!sTouchOk) return;

  uint16_t rawX, rawY;
  i2cBusLock();
  bool pressed = ftReadPoint(rawX, rawY);
  i2cBusUnlock();

  // Touch lock gate: report RELEASED and swallow the point entirely.
  if (!pressed || dataHubGetTouchLocked()) {
    sWasPressed = false;
    return;
  }

  if (rawX >= LCD_WIDTH) rawX = LCD_WIDTH - 1;
  if (rawY >= LCD_HEIGHT) rawY = LCD_HEIGHT - 1;
#if TOUCH_ROT_180
  sLastX = (lv_coord_t)(LCD_WIDTH - 1 - rawX);
  sLastY = (lv_coord_t)(LCD_HEIGHT - 1 - rawY);
#else
  sLastX = (lv_coord_t)rawX;
  sLastY = (lv_coord_t)rawY;
#endif
  data->point.x = sLastX;
  data->point.y = sLastY;
  data->state = LV_INDEV_STATE_PRESSED;

  // Debug: log the first point of each touch so orientation problems are
  // visible over serial (raw = controller output, mapped = what LVGL sees).
  if (!sWasPressed) {
    Serial.printf("[touch] raw %u,%u -> mapped %d,%d\n", rawX, rawY,
                  (int)sLastX, (int)sLastY);
  }
  sWasPressed = true;
}

bool touchInputInit() {
  // Hardware reset, then probe the chip ID. The power task is already
  // polling the PMIC on this bus, so every transaction holds the mutex.
  pinMode(TP_INT, INPUT);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(120);

  i2cBusLock();
  Wire.beginTransmission(kFtAddr);
  Wire.write(kRegChipId);
  bool ok = Wire.endTransmission(false) == 0 &&
            Wire.requestFrom(kFtAddr, (uint8_t)1) == 1;
  uint8_t chipId = ok ? Wire.read() : 0;
  i2cBusUnlock();

  sTouchOk = ok;
  if (!sTouchOk) {
    Serial.println("[touch] FT3168 not found on I2C!");
    return false;
  }
  Serial.printf("[touch] FT3168 online, chip id 0x%02x\n", chipId);

  static lv_indev_drv_t sIndevDrv;
  lv_indev_drv_init(&sIndevDrv);
  sIndevDrv.type = LV_INDEV_TYPE_POINTER;
  sIndevDrv.read_cb = touchReadCb;
  lv_indev_drv_register(&sIndevDrv);
  return true;
}
