#include "i2c_bus.h"

SemaphoreHandle_t i2cBusMutex() {
  // C++11 guarded static: safe even if the first two callers race (power
  // task vs touch init).
  static SemaphoreHandle_t sMutex = xSemaphoreCreateMutex();
  return sMutex;
}
