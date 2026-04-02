#pragma once

#include <Arduino.h>

namespace app::sensing {

enum class SensorKind : uint8_t {
  Unknown = 0,
  Dht = 1,
  Mmwave = 2,
  Battery = 3,
  Wakeup = 4,
};

struct DhtSampleData {
  float temperatureC = 0.0f;
  float humidityPercent = 0.0f;
};

struct MmwaveSampleData {
  bool detected = false;
  bool hasDistance = false;
  uint16_t distanceCm = 0;
  uint16_t frameCount = 0;
  uint16_t byteCount = 0;
};

struct SensorSample {
  SensorKind kind = SensorKind::Unknown;
  uint32_t timestampMs = 0;
  bool valid = false;

  DhtSampleData dht = {};
  MmwaveSampleData mmwave = {};
};

}  // namespace app::sensing
