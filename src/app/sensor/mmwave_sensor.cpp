#include "mmwave_sensor.h"

#include <HardwareSerial.h>
#include <Preferences.h>
#include <app_config.h>
#include <esp_log.h>

namespace app::sensor {

namespace {

static const char* TAG = "mmwave_sensor";
static constexpr uint8_t FRAME_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t FRAME_TAIL[4] = {0xF8, 0xF7, 0xF6, 0xF5};
static constexpr uint16_t MIN_PERIODIC_REPORT_PAYLOAD = 13;
static constexpr uint32_t NO_DATA_LOG_INTERVAL_MS = 5000;
static constexpr uint32_t FRAME_LOG_INTERVAL_MS = 2000;
static constexpr uint32_t MALFORMED_LOG_INTERVAL_MS = 3000;
static constexpr const char* NVS_NAMESPACE = "mmwave_cfg";
static constexpr const char* NVS_KEY_MAX_CM = "max_cm";

HardwareSerial mmwaveUart(1);

}  // namespace

MmwaveSensor mmwaveSensor;

namespace {

uint16_t clampDetectionRange(uint16_t cm) {
  if (cm > static_cast<uint16_t>(MMWAVE_MAX_ALLOWED_DISTANCE_CM)) {
    return static_cast<uint16_t>(MMWAVE_MAX_ALLOWED_DISTANCE_CM);
  }
  return cm;
}

}  // namespace

bool MmwaveSensor::begin(uint8_t rxPin, uint8_t txPin, uint32_t baudrate) {
  if (rxPin == 255 || txPin == 255 || baudrate == 0) {
    ESP_LOGE(TAG, "Invalid mmWave UART config");
    return false;
  }

  rx = rxPin;
  tx = txPin;
  baud = baudrate;

  mmwaveUart.begin(baud, SERIAL_8N1, rx, tx);

  started = true;
  resetParser();
  detected = false;
  hasDistanceValue = false;
  distanceValueCm = 0;
  maxDetectionCm = clampDetectionRange(static_cast<uint16_t>(MMWAVE_DEFAULT_MAX_DISTANCE_CM));
  uint16_t persistedMaxCm = 0;
  const bool loadedFromNvs = loadSettingsFromNvs(persistedMaxCm);
  if (loadedFromNvs) {
    maxDetectionCm = persistedMaxCm;
  }
  targetStateValue = 0;
  reportTypeValue = 0;
  frameCount = 0;
  byteCount = 0;
  const uint32_t now = millis();
  lastByteRxMs = now;
  lastNoDataLogMs = now;
  lastFrameLogMs = 0;
  lastMalformedLogMs = 0;

  ESP_LOGI(TAG, "mmWave UART ready RX=%u TX=%u baud=%lu", rx, tx, static_cast<unsigned long>(baud));
  ESP_LOGI(TAG,
           "mmWave range max=%ucm source=%s",
           static_cast<unsigned>(maxDetectionCm),
           loadedFromNvs ? "nvs" : "profile");
  return true;
}

void MmwaveSensor::resetParser() {
  parseState = ParseState::SyncHeader;
  headerMatched = 0;
  lengthIndex = 0;
  expectedPayloadLength = 0;
  payloadIndex = 0;
  tailIndex = 0;
}

void MmwaveSensor::applyPayload(const uint8_t* payload, uint16_t payloadLength) {
  if (payload == nullptr || payloadLength < MIN_PERIODIC_REPORT_PAYLOAD) {
    return;
  }

  const uint8_t targetState = payload[2];
  const uint8_t reportType = payload[0];
  const uint16_t movingDistanceCm = static_cast<uint16_t>(payload[3]) |
                                    (static_cast<uint16_t>(payload[4]) << 8);
  const uint16_t stationaryDistanceCm = static_cast<uint16_t>(payload[6]) |
                                        (static_cast<uint16_t>(payload[7]) << 8);
  const uint16_t detectionDistanceCm = static_cast<uint16_t>(payload[9]) |
                                       (static_cast<uint16_t>(payload[10]) << 8);

  detected = (targetState != 0);
  targetStateValue = targetState;
  reportTypeValue = reportType;

  if (detectionDistanceCm > 0) {
    distanceValueCm = detectionDistanceCm;
    hasDistanceValue = true;
  } else if (stationaryDistanceCm > 0) {
    distanceValueCm = stationaryDistanceCm;
    hasDistanceValue = true;
  } else if (movingDistanceCm > 0) {
    distanceValueCm = movingDistanceCm;
    hasDistanceValue = true;
  } else {
    distanceValueCm = 0;
    hasDistanceValue = false;
  }

  if (maxDetectionCm > 0 && hasDistanceValue && distanceValueCm > maxDetectionCm) {
    detected = false;
    hasDistanceValue = false;
    distanceValueCm = 0;
    targetStateValue = 0;
  }
}

bool MmwaveSensor::feedByte(uint8_t byte) {
  switch (parseState) {
    case ParseState::SyncHeader: {
      if (byte == FRAME_HEADER[headerMatched]) {
        headerMatched++;
      } else {
        headerMatched = (byte == FRAME_HEADER[0]) ? 1 : 0;
      }

      if (headerMatched == sizeof(FRAME_HEADER)) {
        parseState = ParseState::ReadLength;
        lengthIndex = 0;
      }
      return false;
    }

    case ParseState::ReadLength: {
      lengthBytes[lengthIndex++] = byte;
      if (lengthIndex < sizeof(lengthBytes)) {
        return false;
      }

      expectedPayloadLength = static_cast<uint16_t>(lengthBytes[0]) |
                              (static_cast<uint16_t>(lengthBytes[1]) << 8);
      if (expectedPayloadLength == 0 || expectedPayloadLength > PAYLOAD_BUFFER_SIZE) {
        const uint32_t now = millis();
        if ((now - lastMalformedLogMs) >= MALFORMED_LOG_INTERVAL_MS) {
          ESP_LOGW(TAG,
                   "Invalid payload length=%u (buffer=%u)",
                   static_cast<unsigned>(expectedPayloadLength),
                   static_cast<unsigned>(PAYLOAD_BUFFER_SIZE));
          lastMalformedLogMs = now;
        }
        resetParser();
        return false;
      }

      payloadIndex = 0;
      parseState = ParseState::ReadPayload;
      return false;
    }

    case ParseState::ReadPayload: {
      payloadBuffer[payloadIndex++] = byte;
      if (payloadIndex >= expectedPayloadLength) {
        tailIndex = 0;
        parseState = ParseState::ReadTail;
      }
      return false;
    }

    case ParseState::ReadTail: {
      if (byte != FRAME_TAIL[tailIndex]) {
        const uint32_t now = millis();
        if ((now - lastMalformedLogMs) >= MALFORMED_LOG_INTERVAL_MS) {
          ESP_LOGW(TAG, "Tail mismatch at index=%u", static_cast<unsigned>(tailIndex));
          lastMalformedLogMs = now;
        }
        resetParser();
        return false;
      }

      tailIndex++;
      if (tailIndex < sizeof(FRAME_TAIL)) {
        return false;
      }

      applyPayload(payloadBuffer, expectedPayloadLength);
      if (frameCount < 65535) {
        frameCount++;
      }
      resetParser();
      return true;
    }
  }

  return false;
}

void MmwaveSensor::poll() {
  if (!started) {
    return;
  }

  const uint32_t pollStartMs = millis();
  const uint16_t frameBefore = frameCount;
  const uint16_t bytesBefore = byteCount;

  while (mmwaveUart.available() > 0) {
    const int raw = mmwaveUart.read();
    if (raw < 0) {
      break;
    }

    const uint8_t byte = static_cast<uint8_t>(raw);

    if (byteCount < 65535) {
      byteCount++;
    }

    lastByteRxMs = millis();

    feedByte(byte);
  }

  const bool gotBytes = byteCount != bytesBefore;
  const bool gotFrame = frameCount != frameBefore;

  if (gotFrame && (pollStartMs - lastFrameLogMs) >= FRAME_LOG_INTERVAL_MS) {
    ESP_LOGI(TAG,
             "Frame parsed targetState=%u reportType=%u detected=%u hasDistance=%u distance=%ucm frames=%u bytes=%u",
             static_cast<unsigned>(targetStateValue),
             static_cast<unsigned>(reportTypeValue),
             detected ? 1U : 0U,
             hasDistanceValue ? 1U : 0U,
             static_cast<unsigned>(distanceValueCm),
             static_cast<unsigned>(frameCount),
             static_cast<unsigned>(byteCount));
    lastFrameLogMs = pollStartMs;
  }

  if (!gotBytes && (pollStartMs - lastByteRxMs) >= NO_DATA_LOG_INTERVAL_MS &&
      (pollStartMs - lastNoDataLogMs) >= NO_DATA_LOG_INTERVAL_MS) {
    ESP_LOGW(TAG,
             "No UART bytes from mmWave for %lu ms (RX=%u TX=%u baud=%lu)",
             static_cast<unsigned long>(pollStartMs - lastByteRxMs),
             static_cast<unsigned>(rx),
             static_cast<unsigned>(tx),
             static_cast<unsigned long>(baud));
    lastNoDataLogMs = pollStartMs;
  }
}

bool MmwaveSensor::snapshot(MmwaveReading& out, bool resetCounters) {
  out.valid = started;
  if (!started) {
    return false;
  }

  out.targetDetected = detected;
  out.hasDistance = hasDistanceValue;
  out.distanceCm = distanceValueCm;
  out.targetState = targetStateValue;
  out.reportType = reportTypeValue;
  out.frames = frameCount;
  out.bytes = byteCount;

  if (resetCounters) {
    frameCount = 0;
    byteCount = 0;
  }

  return true;
}

bool MmwaveSensor::setMaxDetectionRangeCm(uint16_t maxDistanceCmInput, bool persistToNvs) {
  const uint16_t nextMaxCm = clampDetectionRange(maxDistanceCmInput);
  maxDetectionCm = nextMaxCm;

  bool persisted = true;
  if (persistToNvs) {
    persisted = storeSettingsToNvs(nextMaxCm);
  }

  ESP_LOGI(TAG,
           "mmWave range updated max=%ucm persist=%u stored=%u",
           static_cast<unsigned>(nextMaxCm),
           persistToNvs ? 1U : 0U,
           persisted ? 1U : 0U);

  return persisted;
}

uint16_t MmwaveSensor::maxDetectionRangeCm() const {
  return maxDetectionCm;
}

bool MmwaveSensor::loadSettingsFromNvs(uint16_t& outMaxDistanceCm) const {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    return false;
  }

  if (!prefs.isKey(NVS_KEY_MAX_CM)) {
    prefs.end();
    return false;
  }

  outMaxDistanceCm = clampDetectionRange(prefs.getUShort(NVS_KEY_MAX_CM, 0));
  prefs.end();
  return true;
}

bool MmwaveSensor::storeSettingsToNvs(uint16_t maxDistanceCmInput) const {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    return false;
  }

  const uint16_t maxDistanceCm = clampDetectionRange(maxDistanceCmInput);
  const bool ok = prefs.putUShort(NVS_KEY_MAX_CM, maxDistanceCm) == maxDistanceCm;
  prefs.end();
  return ok;
}

}  // namespace app::sensor
