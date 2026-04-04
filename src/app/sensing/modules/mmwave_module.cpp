#include "mmwave_module.h"

#include "app/espnow/state_binary.h"
#include "app/sensor/mmwave_sensor.h"

#include <app_config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace app::sensing {

namespace {

void fillSampleFromReading(const app::sensor::MmwaveReading& reading, SensorSample& out) {
  out = {};
  out.kind = SensorKind::Mmwave;
  out.timestampMs = millis();
  out.valid = true;
  out.mmwave.detected = reading.targetDetected;
  out.mmwave.hasDistance = reading.hasDistance;
  out.mmwave.distanceCm = reading.distanceCm;
  out.mmwave.targetState = reading.targetState;
  out.mmwave.reportType = reading.reportType;
  out.mmwave.frameCount = reading.frames;
  out.mmwave.byteCount = reading.bytes;
}

}  // namespace

const char* MmwaveSensorModule::id() const {
  return "mmwave";
}

uint32_t MmwaveSensorModule::featureBit() const {
#if MMWAVE_SENSOR_ENABLED
  return static_cast<uint32_t>(app::espnow::state_binary::FeatureMmwave);
#else
  return 0;
#endif
}

bool MmwaveSensorModule::begin() {
#if MMWAVE_SENSOR_ENABLED
  if (!started) {
    started = app::sensor::mmwaveSensor.begin(MMWAVE_UART_RX_PIN, MMWAVE_UART_TX_PIN, MMWAVE_UART_BAUDRATE);
    lastReadMs = millis();
    presenceFilter.configure(MMWAVE_PRESENCE_ON_CONSECUTIVE,
                             MMWAVE_PRESENCE_OFF_CONSECUTIVE,
                             MMWAVE_PRESENCE_OFF_HOLD_MS);
    presenceFilter.reset(false);
    distanceFilter.setWindowSize(MMWAVE_DISTANCE_MEDIAN_WINDOW);
    distanceFilter.reset();
  }
  return started;
#else
  started = false;
  return true;
#endif
}

void MmwaveSensorModule::poll() {
#if MMWAVE_SENSOR_ENABLED
  if (started) {
    app::sensor::mmwaveSensor.poll();
  }
#endif
}

bool MmwaveSensorModule::readSample(SensorSample& out) {
#if !MMWAVE_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!started) {
    return false;
  }

  const uint32_t now = millis();
  if ((now - lastReadMs) < MMWAVE_READ_INTERVAL_MS) {
    return false;
  }
  lastReadMs = now;

  app::sensor::MmwaveReading reading;
  if (!app::sensor::mmwaveSensor.snapshot(reading, true) || !reading.valid) {
    return false;
  }

  fillSampleFromReading(reading, out);
  out.timestampMs = now;

#if MMWAVE_FILTER_ENABLED
  out.mmwave.detected = presenceFilter.update(reading.targetDetected, now);

  if (reading.hasDistance) {
    distanceFilter.push(reading.distanceCm);
  }
  if (distanceFilter.hasValue()) {
    out.mmwave.hasDistance = true;
    out.mmwave.distanceCm = distanceFilter.median();
  }
#endif

  return true;
#endif
}

bool MmwaveSensorModule::bootSample(SensorSample& out) {
#if !MMWAVE_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!started) {
    return false;
  }

  const uint32_t sampleStart = millis();
  while ((millis() - sampleStart) < MMWAVE_SAMPLE_WINDOW_MS) {
    app::sensor::mmwaveSensor.poll();
    vTaskDelay(pdMS_TO_TICKS(MMWAVE_POLL_INTERVAL_MS));
  }

  app::sensor::MmwaveReading reading;
  if (!app::sensor::mmwaveSensor.snapshot(reading, true) || !reading.valid) {
    return false;
  }

  fillSampleFromReading(reading, out);

#if MMWAVE_FILTER_ENABLED
  out.mmwave.detected = presenceFilter.update(reading.targetDetected, out.timestampMs);

  if (reading.hasDistance) {
    distanceFilter.push(reading.distanceCm);
  }
  if (distanceFilter.hasValue()) {
    out.mmwave.hasDistance = true;
    out.mmwave.distanceCm = distanceFilter.median();
  }
#endif

  return true;
#endif
}

}  // namespace app::sensing
