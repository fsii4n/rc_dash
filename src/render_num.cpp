#include "render_num.h"

#include <stdio.h>
#include <stdlib.h>

#include "rc_monitor.h"

void numFmtLapTime(char *buf, size_t n, int32_t deciSeconds) {
  if (deciSeconds == RC_INVALID_VALUE || deciSeconds < 0) {
    snprintf(buf, n, "-:--.-");
    return;
  }
  int32_t tenths = deciSeconds % 10;
  int32_t total = deciSeconds / 10;
  snprintf(buf, n, "%ld:%02ld.%ld", (long)(total / 60), (long)(total % 60),
           (long)tenths);
}

void numFmtDeltaSec(char *buf, size_t n, int32_t centiSeconds) {
  if (centiSeconds == RC_INVALID_VALUE) {
    snprintf(buf, n, "-.--");
    return;
  }
  snprintf(buf, n, "%s%ld.%02ld", centiSeconds < 0 ? "-" : "+",
           (long)(labs(centiSeconds) / 100), (long)(labs(centiSeconds) % 100));
}

void numFmtSpeedKmh(char *buf, size_t n, int32_t rawMs10) {
  if (rawMs10 == RC_INVALID_VALUE) {
    snprintf(buf, n, "--");
    return;
  }
  snprintf(buf, n, "%d", (int)(rawMs10 * 0.36f + 0.5f));
}
