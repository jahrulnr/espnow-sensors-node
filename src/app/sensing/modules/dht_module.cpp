#include "dht_module.h"

#include "app/espnow/state_binary.h"
#include "app/sensor/dht_sensor.h"

#include <app_config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace app::sensing {

#ifndef DHT_BOOT_SAMPLE_RETRY_COUNT
#define DHT_BOOT_SAMPLE_RETRY_COUNT 5
#endif

const char* DhtSensorModule::id() const {
  return "dht";
}

uint32_t DhtSensorModule::featureBit() const {
#if DHT_SENSOR_ENABLED
  return static_cast<uint32_t>(app::espnow::state_binary::FeatureSensor);
#else
  return 0;
#endif
}

bool DhtSensorModule::begin() {
#if DHT_SENSOR_ENABLED
  if (!started) {
    started = app::sensor::dhtSensor.begin(DHT_SENSOR_PIN, DHT_SENSOR_IS_DHT22 == 1);
    lastReadMs = millis();
  }
  return started;
#else
  started = false;
  return false;
#endif
}

void DhtSensorModule::poll() {
}

bool DhtSensorModule::readSample(SensorSample& out) {
#if !DHT_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!started) {
    return false;
  }

  const uint32_t now = millis();
  if ((now - lastReadMs) < DHT_READ_INTERVAL_MS) {
    return false;
  }
  lastReadMs = now;

  app::sensor::DhtReading reading;
  if (!app::sensor::dhtSensor.read(reading) || !reading.valid) {
    return false;
  }

  out = {};
  out.kind = SensorKind::Dht;
  out.timestampMs = now;
  out.valid = true;
  out.dht.temperatureC = reading.temperatureC;
  out.dht.humidityPercent = reading.humidityPercent;
  return true;
#endif
}

bool DhtSensorModule::bootSample(SensorSample& out) {
#if !DHT_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!started) {
    return false;
  }

  app::sensor::DhtReading reading = {};
  for (size_t attempt = 0; attempt < DHT_BOOT_SAMPLE_RETRY_COUNT; ++attempt) {
    if (app::sensor::dhtSensor.read(reading) && reading.valid) {
      out = {};
      out.kind = SensorKind::Dht;
      out.timestampMs = millis();
      out.valid = true;
      out.dht.temperatureC = reading.temperatureC;
      out.dht.humidityPercent = reading.humidityPercent;
      return true;
    }

    vTaskDelay(pdMS_TO_TICKS(NODE_DHT_RETRY_DELAY_MS));
  }

  return false;
#endif
}

}  // namespace app::sensing
