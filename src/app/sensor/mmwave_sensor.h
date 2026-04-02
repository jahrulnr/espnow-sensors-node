#pragma once

#include <Arduino.h>

namespace app::sensor {

struct MmwaveReading {
  bool valid = false;
  bool targetDetected = false;
  bool hasDistance = false;
  uint16_t distanceCm = 0;
  uint16_t frames = 0;
  uint16_t bytes = 0;
};

class MmwaveSensor {
 public:
  MmwaveSensor() = default;

  bool begin(uint8_t rxPin, uint8_t txPin, uint32_t baudrate);
  void poll();
  bool snapshot(MmwaveReading& out, bool resetCounters = true);

 private:
  bool parseLine(const String& line);
  void onFrameBoundary();

  bool started = false;
  uint8_t rx = 255;
  uint8_t tx = 255;
  uint32_t baud = 0;

  static constexpr size_t LINE_BUFFER_SIZE = 96;
  char lineBuffer[LINE_BUFFER_SIZE] = {0};
  size_t lineLength = 0;

  uint32_t lastByteMs = 0;
  uint32_t frameLastByteMs = 0;
  bool frameOpen = false;

  bool detected = false;
  bool hasDistanceValue = false;
  uint16_t distanceValueCm = 0;

  uint16_t frameCount = 0;
  uint16_t byteCount = 0;
};

extern MmwaveSensor mmwaveSensor;

}  // namespace app::sensor
