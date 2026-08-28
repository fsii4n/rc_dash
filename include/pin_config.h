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
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

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
