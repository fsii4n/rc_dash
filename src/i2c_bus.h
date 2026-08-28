// Shared I2C bus arbitration. The AXP2101 PMIC (power_mon task, core 0) and
// the CST9217 touch controller (LVGL indev read, core 1) share one Wire bus,
// and Wire is not thread-safe: every I2C transaction must happen inside
// i2cBusLock()/i2cBusUnlock(). Keep hold times short — a handful of register
// reads, never a vTaskDelay.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Returns the process-wide I2C bus mutex, creating it on first use
// (thread-safe function-local static).
SemaphoreHandle_t i2cBusMutex();

static inline void i2cBusLock() {
  xSemaphoreTake(i2cBusMutex(), portMAX_DELAY);
}

static inline void i2cBusUnlock() { xSemaphoreGive(i2cBusMutex()); }
