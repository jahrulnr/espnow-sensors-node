#pragma once

#include <Arduino.h>

namespace app::algorithms {

class MedianWindowU16 {
 public:
  static constexpr size_t kMaxWindowSize = 9;

  void setWindowSize(size_t size) {
    const size_t clamped = size < 1 ? 1 : (size > kMaxWindowSize ? kMaxWindowSize : size);
    windowSize = clamped;
    reset();
  }

  void reset() {
    count = 0;
    index = 0;
    for (size_t i = 0; i < kMaxWindowSize; ++i) {
      values[i] = 0;
    }
  }

  void push(uint16_t value) {
    values[index] = value;
    index = (index + 1) % windowSize;
    if (count < windowSize) {
      count++;
    }
  }

  bool hasValue() const {
    return count > 0;
  }

  uint16_t median() const {
    if (count == 0) {
      return 0;
    }

    uint16_t scratch[kMaxWindowSize] = {0};
    for (size_t i = 0; i < count; ++i) {
      scratch[i] = values[i];
    }

    for (size_t i = 1; i < count; ++i) {
      const uint16_t key = scratch[i];
      size_t j = i;
      while (j > 0 && scratch[j - 1] > key) {
        scratch[j] = scratch[j - 1];
        --j;
      }
      scratch[j] = key;
    }

    return scratch[count / 2];
  }

 private:
  size_t windowSize = 5;
  size_t count = 0;
  size_t index = 0;
  uint16_t values[kMaxWindowSize] = {0};
};

}  // namespace app::algorithms
