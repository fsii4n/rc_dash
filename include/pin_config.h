// Pin map for Waveshare ESP32-S3-Touch-AMOLED-1.75C
// Source: waveshareteam/ESP32-S3-Touch-AMOLED-1.75C examples/arduino/libraries/Mylibrary/pin_config.h
#pragma once

#define XPOWERS_CHIP_AXP2101

// CO5300 AMOLED, QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_RESET 1
#define LCD_CS 12
// Tearing-effect output (schematic net LCD_TE: U1 pin 18 / GPIO13). Pulses
// each v-blank once the panel's TEON (0x35) command is sent.
#define LCD_TE 13
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Buttons. Key1 = BOOT key at the 5 o'clock position: schematic net GPIO0
// connects Key1 (other side GND), R7 10K pull-up to VCC3V3, C14 100nF and
// ESP32-S3 pin 5 (GPIO0) — active low, external RC debounce on board.
// Key2 = PWR key (1 o'clock) goes to the AXP2101 PWRON pin, not a GPIO.
#define KEY_BOOT 0

// CST9217 touch + shared I2C bus (AXP2101 PMIC, QMI8658 IMU)
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RST 2

// Audio (unused in this PoC)
#define PIN_ES7210_BCLK 9
#define PIN_ES7210_LRCK 45
#define PIN_ES7210_DIN 10
#define PIN_ES7210_MCLK 16
#define PIN_ES8311_DOUT 8
#define PA 46
