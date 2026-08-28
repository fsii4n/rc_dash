// RaceChrono BLE monitor display — PoC for Waveshare ESP32-S3-Touch-AMOLED-1.75C
//
// Data path: VBOX Sport --SPP--> phone (RaceChrono) --BLE--> this board.
// The board advertises as a "RaceChrono DIY" device; RaceChrono pushes the
// configured channels (GPS speed + lap timing) which are rendered with LVGL.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "data_hub.h"
#include "display_ctl.h"
#include "pin_config.h"
#include "power_mon.h"
#include "rc_monitor.h"
#include "touch_input.h"
#include "ui.h"

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

static Arduino_CO5300 *gfx = new Arduino_CO5300(
    bus, LCD_RESET, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT,
    6 /* col offset 1 */, 0, 0, 0);

static lv_disp_draw_buf_t sDrawBuf;
static uint8_t sBrightness = 200;

void displaySetBrightness(uint8_t value) {
  sBrightness = value;
  gfx->setBrightness(value);
}

uint8_t displayGetBrightness() { return sBrightness; }

static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area,
                      lv_color_t *pixels) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  // LV_COLOR_16_SWAP=1: LVGL renders big-endian RGB565, so this resolves to
  // Arduino_TFT::draw16bitBeRGBBitmap -> writeAddrWindow + bus writeBytes,
  // which DMAs straight out of the LVGL draw buffer (no per-pixel swap, no
  // bounce buffer copy).
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&pixels->full, w,
                            h);
  lv_disp_flush_ready(disp);
}

// CO5300 requires flush areas aligned to even start / odd end coordinates.
static void dispRounder(lv_disp_drv_t *disp, lv_area_t *area) {
  if (area->x1 % 2 != 0) area->x1--;
  if (area->y1 % 2 != 0) area->y1--;
  if (area->x2 % 2 == 0) area->x2++;
  if (area->y2 % 2 == 0) area->y2++;
}

// LVGL render task, pinned to core 1. Pure consumer: pulls the DashModel
// the data hub task (core 0) publishes and drives the UI plugins. All LVGL
// calls happen on this task (touch/slider callbacks run inside
// lv_timer_handler here too) — display actions from other tasks must go
// through DashModel, never call LVGL directly.
static void lvglTask(void *arg) {
  uint32_t lastUiUpdate = 0;
  for (;;) {
    uint32_t now = millis();
    if (now - lastUiUpdate >= 100) {
      lastUiUpdate = now;
      uiTick();
    }
    uint32_t t0 = millis();
    lv_timer_handler();
    uint32_t frameMs = millis() - t0;

    // Telemetry: worst render+flush and stack headroom over each 10s window.
    static uint32_t sMaxFrameMs = 0, sLastReport = 0;
    if (frameMs > sMaxFrameMs) sMaxFrameMs = frameMs;
    if (now - sLastReport >= 10000) {
      if (sLastReport != 0) {
        Serial.printf("[ui] max frame %lums over last 10s, stack free %u\n",
                      (unsigned long)sMaxFrameMs,
                      (unsigned)uxTaskGetStackHighWaterMark(NULL));
      }
      sLastReport = now;
      sMaxFrameMs = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[main] RaceChrono AMOLED monitor PoC");

  powerMonStart();

  if (!gfx->begin(80000000L /* QSPI clock */)) {
    Serial.println("[main] gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setBrightness(sBrightness);

  lv_init();

  // Two quarter-screen buffers in internal DMA-capable RAM.
  size_t bufPixels = LCD_WIDTH * LCD_HEIGHT / 4;
  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
      bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
      bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
  if (!buf1) {
    Serial.println("[main] draw buffer alloc failed!");
    for (;;) delay(1000);
  }
  lv_disp_draw_buf_init(&sDrawBuf, buf1, buf2, bufPixels);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = LCD_WIDTH;
  dispDrv.ver_res = LCD_HEIGHT;
  dispDrv.flush_cb = dispFlush;
  dispDrv.rounder_cb = dispRounder;
  dispDrv.draw_buf = &sDrawBuf;
  lv_disp_drv_register(&dispDrv);

  uiCreate();

  // After display registration (the indev binds to the default display) and
  // before the LVGL task exists, so indev registration can't race
  // lv_timer_handler. The power task is already polling the shared I2C bus;
  // touchInputInit serializes against it via the i2c_bus mutex.
  touchInputInit();

  rcMonitorStart();
  dataHubStart();

  xTaskCreatePinnedToCore(lvglTask, "lvgl", 8192, nullptr, 4, nullptr, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
