// Numeric rendering module: formats quantitative channel values (times,
// speeds, deltas) into display strings. Pure functions, no LVGL — shared by
// any plugin that shows a number. Invalid inputs render as placeholders.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Lap time from deci-seconds: "1:23.4", invalid -> "-:--.-"
void numFmtLapTime(char *buf, size_t n, int32_t deciSeconds);

// Signed delta from centiseconds: "+1.23" / "-0.45", invalid -> "-.--"
void numFmtDeltaSec(char *buf, size_t n, int32_t centiSeconds);

// Speed from m/s*10 to km/h: "132", invalid -> "--"
void numFmtSpeedKmh(char *buf, size_t n, int32_t rawMs10);
