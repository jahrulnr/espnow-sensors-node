#pragma once

#include <Arduino.h>

namespace app::power {

void touchMasterActivity();
void beginMasterTask();
void endMasterTask();

bool canEnterSleep(uint32_t nowMs, uint32_t idleThresholdMs);
uint32_t idleDurationMs(uint32_t nowMs);
uint32_t activeTaskCount();

}  // namespace app::power
