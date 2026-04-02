#pragma once

#include <Arduino.h>

namespace app::algorithms {

inline float trimmedMeanU16(uint16_t* values, size_t count, uint8_t trimPercent) {
  if (values == nullptr || count == 0) {
    return 0.0f;
  }

  for (size_t i = 1; i < count; ++i) {
    const uint16_t key = values[i];
    size_t j = i;
    while (j > 0 && values[j - 1] > key) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = key;
  }

  const uint8_t clampedTrim = trimPercent > 40 ? 40 : trimPercent;
  size_t trimEach = (count * static_cast<size_t>(clampedTrim)) / 100U;
  if ((trimEach * 2U) >= count) {
    trimEach = (count - 1U) / 2U;
  }

  const size_t begin = trimEach;
  const size_t end = count - trimEach;
  if (begin >= end) {
    return static_cast<float>(values[count / 2]);
  }

  uint32_t sum = 0;
  for (size_t i = begin; i < end; ++i) {
    sum += static_cast<uint32_t>(values[i]);
  }

  const size_t used = end - begin;
  if (used == 0) {
    return static_cast<float>(values[count / 2]);
  }

  return static_cast<float>(sum) / static_cast<float>(used);
}

}  // namespace app::algorithms
