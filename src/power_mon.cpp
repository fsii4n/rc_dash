#include "power_mon.h"

#include <Arduino.h>
#include <Wire.h>

#include "pin_config.h"
// XPOWERS_CHIP_AXP2101 comes from build_flags
#include <XPowersLib.h>

static XPowersPMU sPmu;
static portMUX_TYPE sLock = portMUX_INITIALIZER_UNLOCKED;
static PowerStatus sStatus = {};

void powerMonGet(PowerStatus &out) {
  portENTER_CRITICAL(&sLock);
  out = sStatus;
  portEXIT_CRITICAL(&sLock);
}

static void powerTask(void *arg) {
  for (;;) {
    PowerStatus s;
    s.pmicOk = true;
    s.battConnected = sPmu.isBatteryConnect();
    s.percent = s.battConnected ? sPmu.getBatteryPercent() : -1;
    s.voltageMv = sPmu.getBattVoltage();
    s.charging = sPmu.isCharging();
    portENTER_CRITICAL(&sLock);
    sStatus = s;
    portEXIT_CRITICAL(&sLock);

    // The PMIC IRQ line is not routed to a GPIO in our pin map, so poll the
    // interrupt status register instead (same approach as Waveshare's demo).
    // Shutdown requires a 2 s long press; a short press does nothing (its
    // IRQ is not even enabled — reserved for a future function).
    sPmu.getIrqStatus();
    if (sPmu.isPekeyLongPressIrq()) {
      Serial.println("[pwr] power key held 2s, shutting down");
      sPmu.clearIrqStatus();
      delay(100);
      sPmu.shutdown();  // cuts all rails; PWR button press powers back on
    }
    sPmu.clearIrqStatus();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void powerMonStart() {
  Wire.begin(IIC_SDA, IIC_SCL);

  if (!sPmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[pwr] AXP2101 not found on I2C!");
    sStatus.pmicOk = false;
    return;
  }
  Serial.printf("[pwr] AXP2101 online, chip id 0x%02x\n", sPmu.getChipID());

  // Power off on a 2 s PWR-key long press. IRQLEVEL sets when the long-press
  // IRQ fires; the AXP2101 offers 1 / 1.5 / 2 / 2.5 s and 2 s is available
  // exactly (XPOWERS_AXP2101_IRQ_TIME_2S).
  sPmu.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_2S);
  // Hardware forced power-off (OFFLEVEL) stays as a failsafe if the firmware
  // hangs; 4 s is its minimum and comfortably above the 2 s IRQ + 500 ms poll,
  // so the software shutdown always fires first.
  sPmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  sPmu.enableLongPressShutdown();  // OFFLEVEL hold hard-cuts power...
  sPmu.setLongPressPowerOFF();     // ...as a power-off, not a restart

  sPmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  sPmu.clearIrqStatus();
  sPmu.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ);

  sPmu.enableBattDetection();
  sPmu.enableBattVoltageMeasure();
  sPmu.enableVbusVoltageMeasure();
  sPmu.enableSystemVoltageMeasure();

  xTaskCreatePinnedToCore(powerTask, "pwr", 4096, nullptr, 2, nullptr, 0);
}
