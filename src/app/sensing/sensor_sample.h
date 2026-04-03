#pragma once

#include <Arduino.h>

namespace app::sensing {

enum class SensorKind : uint8_t {
  Unknown = 0,
  Dht = 1,
  Mmwave = 2,
  Camera = 3,
  Battery = 4,
  Wakeup = 5,
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

struct CameraSampleData {
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t frameBytes = 0;
  uint16_t latencyMs = 0;
  uint8_t frameFormat = 0;
  uint8_t cameraType = 0;
};

struct SensorSample {
  SensorKind kind = SensorKind::Unknown;
  uint32_t timestampMs = 0;
  bool valid = false;

  DhtSampleData dht = {};
  MmwaveSampleData mmwave = {};
  CameraSampleData camera = {};
};

}  // namespace app::sensing
