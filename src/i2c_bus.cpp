#include "i2c_bus.h"

SemaphoreHandle_t i2cBusMutex() {
  // C++11 guarded static: safe even if the first two callers race (power
  // task on core 0 vs touch init on core 1).
  static SemaphoreHandle_t sMutex = xSemaphoreCreateMutex();
  return sMutex;
}
