#include "networkTask.h"

#include "app/espnow/slave.h"
#include "app/espnow/state_binary.h"
#include "app/power/sleep_guard.h"
#include "app/sensing/sensor_encoder.h"
#include "app/sensing/sensor_manager.h"

#include <app_config.h>
#include <app/espnow/protocol.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_sleep.h>

namespace app::tasks {

namespace {

static constexpr const char* TAG = "NET_TASK";
static constexpr uint16_t NETWORK_TASK_STACK = 8192;
static constexpr UBaseType_t NETWORK_TASK_PRIORITY = 2;
#if !ENABLE_POWERSAVE
static constexpr size_t OUTGOING_QUEUE_DEPTH = 10;
#endif

RTC_DATA_ATTR bool hasSeenMasterBefore = false;

struct OutgoingJob {
  uint8_t payload[app::espnow::MAX_PAYLOAD_SIZE];
  uint16_t payloadSize;
  bool isText;
};

TaskHandle_t networkTaskHandle = nullptr;
#if !ENABLE_POWERSAVE
QueueHandle_t outgoingQueue = nullptr;
#endif

void sendIdentityStateNow() {
#if NODE_SEND_IDENTITY_STATE
  app::espnow::state_binary::IdentityState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
  strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
  app::espnow::espnowSlave.sendStateBinary(&state, sizeof(state));
#endif
}

void sendFeaturesStateNow() {
  app::espnow::state_binary::FeaturesState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
  state.contractVersion = 1;
  state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
                   | app::sensing::sensorManager.featureBits();
#if ENABLE_WIFI_MODE
  state.featureBits |= static_cast<uint32_t>(app::espnow::state_binary::FeatureWifiSta);
#endif
  app::espnow::espnowSlave.sendStateBinary(&state, sizeof(state));
}

#if ENABLE_POWERSAVE
uint64_t currentSleepDurationUs() {
  if (NODE_EFFECTIVE_SLEEP_MODE == POWERSAVE_SLEEP_MODE_LIGHT) {
    return static_cast<uint64_t>(POWERSAVE_LIGHT_SLEEP_MS) * 1000ULL;
  }
  return static_cast<uint64_t>(POWERSAVE_DEEP_SLEEP_SEC) * 1000000ULL;
}

const char* currentSleepModeName() {
  return NODE_EFFECTIVE_SLEEP_MODE == POWERSAVE_SLEEP_MODE_LIGHT ? "light" : "deep";
}

void enterTimedSleep(const char* reason) {
  const uint64_t sleepUs = currentSleepDurationUs();
  ESP_LOGI(TAG,
           "Entering %s sleep for %llu us: %s",
           currentSleepModeName(),
           static_cast<unsigned long long>(sleepUs),
           reason == nullptr ? "n/a" : reason);

  if (esp_sleep_enable_timer_wakeup(sleepUs) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set timer wakeup");
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
  }

  delay(20);

  if (NODE_EFFECTIVE_SLEEP_MODE == POWERSAVE_SLEEP_MODE_LIGHT) {
    const esp_err_t err = esp_light_sleep_start();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_light_sleep_start failed: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    return;
  }

  esp_deep_sleep_start();
}

const char* wakeupCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "cold-boot";
    default:
      return "other";
  }
}

bool waitForMasterLink(uint32_t timeoutMs) {
  const uint32_t startMs = millis();
  while ((millis() - startMs) < timeoutMs) {
    app::espnow::espnowSlave.loop();
    if (app::espnow::espnowSlave.isMasterLinked()) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(NODE_MASTER_POLL_INTERVAL_MS));
  }

  return app::espnow::espnowSlave.isMasterLinked();
}

bool sendIdentityAndFeatures() {
  bool sentAny = false;
  for (uint32_t i = 0; i < NODE_BOOT_ANNOUNCE_REPEATS; ++i) {
    sendIdentityStateNow();
    sentAny = true;
    vTaskDelay(pdMS_TO_TICKS(20));

    sendFeaturesStateNow();
    sentAny = true;
    vTaskDelay(pdMS_TO_TICKS(20));

    if (!app::espnow::espnowSlave.sendModuleListSnapshot()) {
      ESP_LOGW(TAG, "Failed sending proactive module list snapshot (attempt %u)", static_cast<unsigned>(i + 1));
    } else {
      sentAny = true;
    }

    if (i + 1 < NODE_BOOT_ANNOUNCE_REPEATS) {
      vTaskDelay(pdMS_TO_TICKS(NODE_BOOT_ANNOUNCE_GAP_MS));
    }
  }

  vTaskDelay(pdMS_TO_TICKS(20));
  return sentAny;
}

struct PowerSaveSendContext {
  size_t sent = 0;
};

bool sendPowerSaveSample(const app::sensing::SensorSample& sample, void* userData) {
  app::power::touchMasterActivity();

  uint8_t payload[app::espnow::MAX_PAYLOAD_SIZE] = {0};
  size_t payloadSize = 0;
  if (!app::sensing::encodeSampleToStateBinary(sample, payload, sizeof(payload), payloadSize)) {
    ESP_LOGW(TAG, "Encode failed for sample kind=%s", app::sensing::sensorKindName(sample.kind));
    return false;
  }

  if (!app::espnow::espnowSlave.sendStateBinary(payload, payloadSize)) {
    ESP_LOGW(TAG, "Send failed for sample kind=%s", app::sensing::sensorKindName(sample.kind));
    return false;
  }

  auto* context = static_cast<PowerSaveSendContext*>(userData);
  if (context != nullptr) {
    context->sent++;
  }

  vTaskDelay(pdMS_TO_TICKS(NODE_POST_SEND_SETTLE_MS));
  return true;
}

void waitForIdleSleepWindow() {
  while (!app::power::canEnterSleep(millis(), NODE_SLEEP_IDLE_THRESHOLD_MS)) {
    app::espnow::espnowSlave.loop();
    vTaskDelay(pdMS_TO_TICKS(NODE_MASTER_POLL_INTERVAL_MS));
  }
}

size_t sendPowerSaveBootSamples() {
  app::sensing::sensorManager.pollAll();

  PowerSaveSendContext context = {};
  for (uint32_t i = 0; i < NODE_BOOT_SAMPLE_REPEATS; ++i) {
    app::sensing::sensorManager.collectBootSamples(sendPowerSaveSample, &context);
    if (i + 1 < NODE_BOOT_SAMPLE_REPEATS) {
      vTaskDelay(pdMS_TO_TICKS(NODE_POST_SEND_SETTLE_MS));
    }
  }
  return context.sent;
}

void runPowerSaveCycle() {
  const auto cause = esp_sleep_get_wakeup_cause();
  ESP_LOGI(TAG,
           "Wake cycle start: cause=%s master_seen_before=%s",
           wakeupCauseName(cause),
           hasSeenMasterBefore ? "yes" : "no");

  ESP_LOGI(TAG, "Initializing ESP-NOW slave");
  if (!app::espnow::espnowSlave.begin(app::espnow::DEFAULT_CHANNEL, false)) {
    ESP_LOGE(TAG, "ESP-NOW slave init failed");
    enterTimedSleep("espnow init failed");
    return;
  }

  // Force a fresh master link detection in this wake cycle.
  app::espnow::espnowSlave.resetMasterTracking();
  ESP_LOGI(TAG, "Waiting for master link");

  const bool linked = waitForMasterLink(NODE_MASTER_WAIT_TIMEOUT_MS);
  app::espnow::espnowSlave.onWakeCycleLinkResult(linked);

  if (!linked) {
    ESP_LOGW(TAG, "Master not found in %u ms, skipping send", static_cast<unsigned>(NODE_MASTER_WAIT_TIMEOUT_MS));
    enterTimedSleep("master not found");
    return;
  }

  hasSeenMasterBefore = true;
  app::power::touchMasterActivity();
  sendIdentityAndFeatures();

  const uint32_t featureBits = app::sensing::sensorManager.featureBits();
  const bool hasEspNowBootSampleModule =
      (featureBits & static_cast<uint32_t>(app::espnow::state_binary::FeatureSensor)) != 0 ||
      (featureBits & static_cast<uint32_t>(app::espnow::state_binary::FeatureMmwave)) != 0 ||
      (featureBits & static_cast<uint32_t>(app::espnow::state_binary::FeatureCameraJpeg)) != 0;

  const size_t sentSamples = sendPowerSaveBootSamples();
  if (sentSamples == 0) {
    if (hasEspNowBootSampleModule) {
      ESP_LOGW(TAG, "No ESP-NOW boot sample sent before sleep");
    } else {
      ESP_LOGI(TAG, "No ESP-NOW boot sample expected for active modules");
    }
  } else {
    ESP_LOGI(TAG, "Sent %u sensor sample(s) before sleep", static_cast<unsigned>(sentSamples));
  }

  waitForIdleSleepWindow();

  enterTimedSleep("cycle complete");
}
#endif

void networkTaskRunner(void*) {
#if !ENABLE_POWERSAVE
  // start espnow radio
  app::espnow::espnowSlave.begin(app::espnow::DEFAULT_CHANNEL);

  // prepare outgoing queue
  outgoingQueue = xQueueCreate(OUTGOING_QUEUE_DEPTH, sizeof(OutgoingJob));
  if (outgoingQueue == nullptr) {
    ESP_LOGE("NET_TASK", "Failed creating outgoing queue");
  }

  bool wasMasterLinked = false;

  while (true) {
    app::espnow::espnowSlave.loop();

    // handle outgoing queue
    if (outgoingQueue != nullptr) {
      OutgoingJob job;
      if (xQueueReceive(outgoingQueue, &job, 0) == pdTRUE) {
        if (job.payloadSize > 0) {
          app::espnow::espnowSlave.sendStateBinary(job.payload, job.payloadSize);
        }
      }
    }

    // handle master link events
    const bool isMasterLinked = app::espnow::espnowSlave.isMasterLinked();
    if (isMasterLinked && !wasMasterLinked) {
      sendIdentityStateNow();
      sendFeaturesStateNow();
    }
    wasMasterLinked = isMasterLinked;

    vTaskDelay(pdMS_TO_TICKS(10));
  }
#endif
}

}  // namespace

bool startNetworkTask() {
#if ENABLE_POWERSAVE
  while (true) {
    runPowerSaveCycle();
  }
  return false;
#else
  if (networkTaskHandle != nullptr) {
    return true;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
      networkTaskRunner,
      "network_task",
      NETWORK_TASK_STACK,
      nullptr,
      NETWORK_TASK_PRIORITY,
      &networkTaskHandle,
      tskNO_AFFINITY);

  if (created != pdPASS) {
    ESP_LOGE("NET_TASK", "Failed to start network task");
    networkTaskHandle = nullptr;
    return false;
  }

  ESP_LOGI("NET_TASK", "Network task started");
  return true;
#endif
}

bool publishOutgoingBinary(const void* payload, size_t payloadSize) {
#if ENABLE_POWERSAVE
  if (payload == nullptr || payloadSize == 0 || payloadSize > app::espnow::MAX_PAYLOAD_SIZE) {
    return false;
  }

  return app::espnow::espnowSlave.sendStateBinary(payload, payloadSize);
#else
  if (payload == nullptr || payloadSize == 0 || payloadSize > app::espnow::MAX_PAYLOAD_SIZE || outgoingQueue == nullptr) {
    return false;
  }

  OutgoingJob job;
  memset(&job, 0, sizeof(job));
  memcpy(job.payload, payload, payloadSize);
  job.payloadSize = static_cast<uint16_t>(payloadSize);
  job.isText = false;

  if (xQueueSend(outgoingQueue, &job, 0) != pdTRUE) {
    return false;
  }
  return true;
#endif
}

bool publishOutgoingText(const String& text) {
  if (text.isEmpty()) {
    return false;
  }
  return publishOutgoingBinary(text.c_str(), text.length());
}

}  // namespace app::tasks
