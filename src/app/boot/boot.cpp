#include "boot.h"

#include "app/actuation/actuator_manager.h"
#include "app/sensing/sensor_encoder.h"
#include "app/sensing/sensor_manager.h"

#include <esp_log.h>

namespace app::boot {

namespace {

static constexpr const char* TAG = "APP_BOOT";

bool logBootSample(const app::sensing::SensorSample& sample, void*) {
  switch (sample.kind) {
    case app::sensing::SensorKind::Dht:
      ESP_LOGI(TAG,
               "BOOT-SAMPLE dht temp=%.1fC hum=%.1f%%",
               sample.dht.temperatureC,
               sample.dht.humidityPercent);
      return true;
    case app::sensing::SensorKind::Mmwave:
      ESP_LOGI(TAG,
               "BOOT-SAMPLE mmwave detected=%u distance=%ucm frames=%u bytes=%u",
               static_cast<unsigned>(sample.mmwave.detected ? 1 : 0),
               static_cast<unsigned>(sample.mmwave.distanceCm),
               static_cast<unsigned>(sample.mmwave.frameCount),
               static_cast<unsigned>(sample.mmwave.byteCount));
      return true;
    default:
      ESP_LOGI(TAG, "BOOT-SAMPLE %s collected", app::sensing::sensorKindName(sample.kind));
      return true;
  }
}

}  // namespace

bool ensureSensorsReady() {
  return app::sensing::sensorManager.beginAll();
}

bool ensureActuatorsReady() {
  return app::actuation::actuatorManager.beginAll();
}

void run() {
  if (!ensureSensorsReady()) {
    ESP_LOGW(TAG, "One or more sensor modules failed to initialize");
  }
  if (!ensureActuatorsReady()) {
    ESP_LOGW(TAG, "One or more actuator modules failed to initialize");
  }
  app::sensing::sensorManager.collectBootSamples(logBootSample, nullptr);
}

}  // namespace app::boot
