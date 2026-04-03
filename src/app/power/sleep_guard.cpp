#include "sleep_guard.h"

#include <freertos/FreeRTOS.h>

namespace app::power {
namespace {
portMUX_TYPE gMutex = portMUX_INITIALIZER_UNLOCKED;
uint32_t gLastMasterActivityMs = 0;
uint32_t gActiveTaskCount = 0;
bool gInitialized = false;

void ensureInitialized() {
  if (gInitialized) {
    return;
  }

  portENTER_CRITICAL(&gMutex);
  if (!gInitialized) {
    gLastMasterActivityMs = millis();
    gInitialized = true;
  }
  portEXIT_CRITICAL(&gMutex);
}
}  // namespace

void touchMasterActivity() {
  ensureInitialized();
  portENTER_CRITICAL(&gMutex);
  gLastMasterActivityMs = millis();
  portEXIT_CRITICAL(&gMutex);
}

void beginMasterTask() {
  ensureInitialized();
  portENTER_CRITICAL(&gMutex);
  if (gActiveTaskCount < 0xFFFFFFFFU) {
    gActiveTaskCount++;
  }
  gLastMasterActivityMs = millis();
  portEXIT_CRITICAL(&gMutex);
}

void endMasterTask() {
  ensureInitialized();
  portENTER_CRITICAL(&gMutex);
  if (gActiveTaskCount > 0) {
    gActiveTaskCount--;
  }
  gLastMasterActivityMs = millis();
  portEXIT_CRITICAL(&gMutex);
}

uint32_t activeTaskCount() {
  ensureInitialized();
  portENTER_CRITICAL(&gMutex);
  const uint32_t count = gActiveTaskCount;
  portEXIT_CRITICAL(&gMutex);
  return count;
}

uint32_t idleDurationMs(uint32_t nowMs) {
  ensureInitialized();
  portENTER_CRITICAL(&gMutex);
  const uint32_t lastMs = gLastMasterActivityMs;
  portEXIT_CRITICAL(&gMutex);

  return nowMs - lastMs;
}

bool canEnterSleep(uint32_t nowMs, uint32_t idleThresholdMs) {
  if (activeTaskCount() > 0) {
    return false;
  }

  return idleDurationMs(nowMs) >= idleThresholdMs;
}

}  // namespace app::power
