// AXP2101 PMIC monitor: battery gauge + power key handling.
//
// The board's PWR button is wired to the AXP2101 PWRON pin (see
// docs/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf). Power-on is handled in
// hardware by the PMIC; this module adds the software side:
//   - polls the fuel gauge over I2C (percent, voltage, charging state)
//   - a short press of the PWR button triggers a clean shutdown
//     (AXP2101 cuts the output rails; pressing PWR again boots the board)
#pragma once

struct PowerStatus {
  bool pmicOk;         // AXP2101 responded on I2C
  bool battConnected;  // battery detected
  int percent;         // 0..100, valid when battConnected
  int voltageMv;       // battery voltage in mV
  bool charging;       // charger active (USB plugged and charging)
};

// Initializes I2C + PMIC and starts the polling task. Call once from setup().
void powerMonStart();

// Thread-safe copy of the latest PMIC readings.
void powerMonGet(PowerStatus &out);
