#include "mmwave_sensor.h"

#include <HardwareSerial.h>
#include <esp_log.h>

namespace app::sensor {

namespace {

static const char* TAG = "mmwave_sensor";
static constexpr uint32_t FRAME_GAP_MS = 40;

HardwareSerial mmwaveUart(1);

bool containsToken(const String& text, const char* token) {
  return token != nullptr && text.indexOf(token) >= 0;
}

bool parseFirstInteger(const String& text, uint16_t& out) {
  int start = -1;
  for (int i = 0; i < text.length(); ++i) {
    if (isDigit(text[i])) {
      start = i;
      break;
    }
  }

  if (start < 0) {
    return false;
  }

  int end = start;
  while (end < text.length() && isDigit(text[end])) {
    end++;
  }

  const long value = text.substring(start, end).toInt();
  if (value < 0 || value > 65535) {
    return false;
  }

  out = static_cast<uint16_t>(value);
  return true;
}

}  // namespace

MmwaveSensor mmwaveSensor;

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
  lineLength = 0;
  lastByteMs = millis();
  frameLastByteMs = lastByteMs;
  frameOpen = false;
  detected = false;
  hasDistanceValue = false;
  distanceValueCm = 0;
  frameCount = 0;
  byteCount = 0;

  ESP_LOGI(TAG, "mmWave UART ready RX=%u TX=%u baud=%lu", rx, tx, static_cast<unsigned long>(baud));
  return true;
}

void MmwaveSensor::onFrameBoundary() {
  if (!frameOpen) {
    return;
  }

  frameOpen = false;
  frameCount++;
}

bool MmwaveSensor::parseLine(const String& line) {
  String text = line;
  text.trim();
  text.toLowerCase();

  if (text.isEmpty()) {
    return false;
  }

  bool updated = false;

  if (containsToken(text, "presence=1") || containsToken(text, "moving") || containsToken(text, "occupied") ||
      containsToken(text, "detect") || containsToken(text, "target=1") || containsToken(text, "on")) {
    detected = true;
    updated = true;
  }

  if (containsToken(text, "presence=0") || containsToken(text, "idle") || containsToken(text, "clear") ||
      containsToken(text, "target=0") || containsToken(text, "off")) {
    detected = false;
    updated = true;
  }

  if (containsToken(text, "dist") || containsToken(text, "range") || containsToken(text, "cm")) {
    uint16_t parsedDistance = 0;
    if (parseFirstInteger(text, parsedDistance)) {
      distanceValueCm = parsedDistance;
      hasDistanceValue = true;
      updated = true;
    }
  }

  return updated;
}

void MmwaveSensor::poll() {
  if (!started) {
    return;
  }

  const uint32_t now = millis();
  if (frameOpen && (now - frameLastByteMs) > FRAME_GAP_MS) {
    onFrameBoundary();
  }

  while (mmwaveUart.available() > 0) {
    const int raw = mmwaveUart.read();
    if (raw < 0) {
      break;
    }

    const uint8_t byte = static_cast<uint8_t>(raw);
    lastByteMs = now;
    frameLastByteMs = now;
    frameOpen = true;

    if (byteCount < 65535) {
      byteCount++;
    }

    if (byte == '\n' || byte == '\r') {
      if (lineLength > 0) {
        lineBuffer[lineLength] = '\0';
        parseLine(String(lineBuffer));
        lineLength = 0;
      }
      continue;
    }

    if (isPrintable(byte)) {
      if (lineLength + 1 < LINE_BUFFER_SIZE) {
        lineBuffer[lineLength++] = static_cast<char>(byte);
      } else {
        lineBuffer[LINE_BUFFER_SIZE - 1] = '\0';
        parseLine(String(lineBuffer));
        lineLength = 0;
      }
    }
  }
}

bool MmwaveSensor::snapshot(MmwaveReading& out, bool resetCounters) {
  out.valid = started;
  if (!started) {
    return false;
  }

  const uint32_t now = millis();
  if (frameOpen && (now - frameLastByteMs) > FRAME_GAP_MS) {
    onFrameBoundary();
  }

  out.targetDetected = detected;
  out.hasDistance = hasDistanceValue;
  out.distanceCm = distanceValueCm;
  out.frames = frameCount;
  out.bytes = byteCount;

  if (resetCounters) {
    frameCount = 0;
    byteCount = 0;
  }

  return true;
}

}  // namespace app::sensor
