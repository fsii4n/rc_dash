// Display hardware controls, implemented in main.cpp (which owns the gfx
// object). Callable from UI plugins (e.g. the brightness menu).
#pragma once

#include <stdint.h>

// 0..255, forwarded to the CO5300 panel.
void displaySetBrightness(uint8_t value);
uint8_t displayGetBrightness();
