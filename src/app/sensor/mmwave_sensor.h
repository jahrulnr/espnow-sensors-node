#pragma once

#include <Arduino.h>

namespace app::sensor {

struct MmwaveReading {
  bool valid = false;
  bool targetDetected = false;
  bool hasDistance = false;
  uint16_t distanceCm = 0;
  uint8_t targetState = 0;
  uint8_t reportType = 0;
  uint16_t frames = 0;
  uint16_t bytes = 0;
};

class MmwaveSensor {
 public:
  MmwaveSensor() = default;

  bool begin(uint8_t rxPin, uint8_t txPin, uint32_t baudrate);
  void poll();
  bool snapshot(MmwaveReading& out, bool resetCounters = true);
  bool setMaxDetectionRangeCm(uint16_t maxDistanceCm, bool persistToNvs);
  uint16_t maxDetectionRangeCm() const;

 private:
  enum class ParseState : uint8_t {
    SyncHeader = 0,
    ReadLength,
    ReadPayload,
    ReadTail,
  };

  bool feedByte(uint8_t byte);
  void applyPayload(const uint8_t* payload, uint16_t payloadLength);
  void resetParser();
  bool loadSettingsFromNvs(uint16_t& outMaxDistanceCm) const;
  bool storeSettingsToNvs(uint16_t maxDistanceCm) const;

  bool started = false;
  uint8_t rx = 255;
  uint8_t tx = 255;
  uint32_t baud = 0;

  static constexpr uint16_t PAYLOAD_BUFFER_SIZE = 128;

  ParseState parseState = ParseState::SyncHeader;
  uint8_t headerMatched = 0;
  uint8_t lengthBytes[2] = {0};
  uint8_t lengthIndex = 0;
  uint16_t expectedPayloadLength = 0;
  uint8_t payloadBuffer[PAYLOAD_BUFFER_SIZE] = {0};
  uint16_t payloadIndex = 0;
  uint8_t tailIndex = 0;

  bool detected = false;
  bool hasDistanceValue = false;
  uint16_t distanceValueCm = 0;
  uint16_t maxDetectionCm = 0;
  uint8_t targetStateValue = 0;
  uint8_t reportTypeValue = 0;

  uint16_t frameCount = 0;
  uint16_t byteCount = 0;

  uint32_t lastByteRxMs = 0;
  uint32_t lastNoDataLogMs = 0;
  uint32_t lastFrameLogMs = 0;
  uint32_t lastMalformedLogMs = 0;
};

extern MmwaveSensor mmwaveSensor;

}  // namespace app::sensor
