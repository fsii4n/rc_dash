// Pin map for Waveshare ESP32-C6-Touch-AMOLED-2.06
// Sources: waveshareteam/ESP32-C6-Touch-AMOLED-2.06
//   examples/arduino/libraries/Mylibrary/pin_config.h, the official ESP-IDF
//   BSP (waveshare/esp32_c6_touch_amoled_2_06), and the V1.0 schematic.
#pragma once

#define XPOWERS_CHIP_AXP2101

// CO5300 AMOLED, QSPI. 410x502 rectangular panel (rounded corners), the
// panel window starts at column offset 22 (from Waveshare's examples).
// The schematic has an LCD_TE net on the panel connector but it is not
// routed to any usable ESP32-C6 GPIO (the official BSP doesn't use it
// either), so there is no TE-based v-sync on this board.
#define LCD_SDIO0 1
#define LCD_SDIO1 2
#define LCD_SDIO2 3
#define LCD_SDIO3 4
#define LCD_SCLK 0
#define LCD_CS 5
#define LCD_RESET 11
#define LCD_COL_OFFSET 22
#define LCD_WIDTH 410
#define LCD_HEIGHT 502

// Buttons. Key1 = BOOT key: the C6's download-strap GPIO9, active low with
// an external pull-up (schematic KEYS block). Key3 = PWR key goes to the
// AXP2101 PWRON pin (via SYS_OUT), not a GPIO.
#define KEY_BOOT 9

// FT3168 touch + shared I2C bus (AXP2101 PMIC, QMI8658 IMU, PCF85063 RTC)
#define IIC_SDA 8
#define IIC_SCL 7
#define TP_INT 15
#define TP_RST 10

// Audio (ES8311 codec + ES7210 ADC, unused in this PoC)
#define PIN_I2S_MCLK 19
#define PIN_I2S_SCLK 20
#define PIN_I2S_LRCK 22
#define PIN_I2S_DOUT 23
#define PIN_I2S_DSIN 21
#define PA_CTRL 6
