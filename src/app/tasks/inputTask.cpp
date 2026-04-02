#include "inputTask.h"

#include "app/boot/boot.h"
#include "app/espnow/payload_codec.h"
#include "app/input/battery_manager.h"
#include "app/sensing/sensor_encoder.h"
#include "app/sensing/sensor_manager.h"
#include "app/tasks/networkTask.h"
#include "app/espnow/protocol.h"

#include <app_config.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace app::tasks {

namespace {

static constexpr const char* TAG = "INPUT_TASK";
static constexpr uint16_t INPUT_TASK_STACK = 4096;
static constexpr UBaseType_t INPUT_TASK_PRIORITY = 1;
static constexpr uint32_t INPUT_POLL_INTERVAL_MS = 20;
static constexpr uint32_t BATTERY_PUBLISH_INTERVAL_MS = 1000;

TaskHandle_t inputTaskHandle = nullptr;
BatteryManager batteryManager;

uint32_t lastBatteryPublishMs = 0;
int lastPublishedBatteryLevel = -1;

void publishBatterySnapshotToDisplay() {
  batteryManager.update();

  const uint32_t now = millis();
  if (lastBatteryPublishMs != 0 && (now - lastBatteryPublishMs) < BATTERY_PUBLISH_INTERVAL_MS) {
    return;
  }

  const int batteryLevel = batteryManager.getLevel();
  if (batteryLevel < 0 || batteryLevel > 100) {
    return;
  }

  if (batteryLevel == lastPublishedBatteryLevel && lastBatteryPublishMs != 0) {
    return;
  }

  const String payload = app::espnow::codec::buildPayload({
      {"batt", String(batteryLevel)},
  });

  lastPublishedBatteryLevel = batteryLevel;
  lastBatteryPublishMs = now;
}

bool publishSensorSample(const app::sensing::SensorSample& sample, void*) {
  uint8_t payload[app::espnow::MAX_PAYLOAD_SIZE] = {0};
  size_t payloadSize = 0;
  if (!app::sensing::encodeSampleToStateBinary(sample, payload, sizeof(payload), payloadSize)) {
    return false;
  }

  return app::tasks::publishOutgoingBinary(payload, payloadSize);
}

void inputTaskRunner(void*) {
  batteryManager.init(INPUT_BATTERY_ADC_PIN);
  batteryManager.setVoltage(3.3f, 4.2f, 2.0f);
  batteryManager.setUpdateInterval(5000);
  app::boot::ensureSensorsReady();
  
  publishBatterySnapshotToDisplay();

  while (true) {
    publishBatterySnapshotToDisplay();
    app::sensing::sensorManager.pollAll();
    app::sensing::sensorManager.collectSamples(publishSensorSample, nullptr);

    vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_INTERVAL_MS));
  }
}

}  // namespace

bool startInputTask() {
  if (inputTaskHandle != nullptr) {
    return true;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
      inputTaskRunner,
      "input_task",
      INPUT_TASK_STACK,
      nullptr,
      INPUT_TASK_PRIORITY,
      &inputTaskHandle,
      tskNO_AFFINITY);

  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed to start input task");
    inputTaskHandle = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "Input task started");
  return true;
}

}  // namespace app::tasks
