#pragma once

#include <Arduino.h>

namespace app::algorithms {

class BinaryStateFilter {
 public:
  void configure(uint8_t onConsecutive, uint8_t offConsecutive, uint32_t offHoldMs) {
    onConsecutiveThreshold = onConsecutive == 0 ? 1 : onConsecutive;
    offConsecutiveThreshold = offConsecutive == 0 ? 1 : offConsecutive;
    offHoldDurationMs = offHoldMs;
  }

  void reset(bool initialState = false) {
    filteredState = initialState;
    consecutiveOn = 0;
    consecutiveOff = 0;
    lastRawOnMs = 0;
  }

  bool update(bool rawOn, uint32_t nowMs) {
    if (rawOn) {
      if (consecutiveOn < 255) {
        consecutiveOn++;
      }
      consecutiveOff = 0;
      lastRawOnMs = nowMs;

      if (!filteredState && consecutiveOn >= onConsecutiveThreshold) {
        filteredState = true;
      }
      return filteredState;
    }

    if (consecutiveOff < 255) {
      consecutiveOff++;
    }
    consecutiveOn = 0;

    const bool holdExpired = (nowMs - lastRawOnMs) >= offHoldDurationMs;
    if (filteredState && holdExpired && consecutiveOff >= offConsecutiveThreshold) {
      filteredState = false;
    }

    return filteredState;
  }

  bool state() const {
    return filteredState;
  }

 private:
  bool filteredState = false;
  uint8_t consecutiveOn = 0;
  uint8_t consecutiveOff = 0;
  uint32_t lastRawOnMs = 0;

  uint8_t onConsecutiveThreshold = 2;
  uint8_t offConsecutiveThreshold = 2;
  uint32_t offHoldDurationMs = 5000;
};

}  // namespace app::algorithms
