#include "slave_command_hooks.h"

#include "slave.h"
#include "state_binary.h"

#include "app/actuation/actuator_manager.h"
#include "app/network/wifi_manager.h"
#include "app/sensing/sensor_manager.h"

#include <app_config.h>
#include <cstring>
#include <esp_log.h>

namespace app::espnow::hooks {

bool sendIdentityStateNow(SlaveNode& node, const char* logTag) {
#if !NODE_SEND_IDENTITY_STATE
  (void)node;
  (void)logTag;
  return true;
#else
  app::espnow::state_binary::IdentityState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
  strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
  state.id[sizeof(state.id) - 1] = '\0';

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending identity state");
  }
  return sent;
#endif
}

bool sendFeaturesStateNow(SlaveNode& node, const char* logTag) {
  app::espnow::state_binary::FeaturesState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
  state.contractVersion = 1;
  state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
                   | app::sensing::sensorManager.featureBits()
                   | app::actuation::actuatorManager.featureBits();
#if ENABLE_WIFI_MODE
  state.featureBits |= static_cast<uint32_t>(app::espnow::state_binary::FeatureWifiSta);
#endif

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending feature state");
  }
  return sent;
}

void sendServoAckNow(SlaveNode& node,
                     const app::actuation::ActuationResponse& response,
                     const char* logTag) {
  app::espnow::state_binary::ServoAckState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::ServoAck);
  state.ok = response.ok ? 1 : 0;
  state.status = static_cast<uint8_t>(response.status);
  strncpy(state.group, response.servo.group, sizeof(state.group) - 1);
  state.group[sizeof(state.group) - 1] = '\0';
  state.channel = response.servo.channel;
  state.targetDeg10 = response.servo.targetDeg10;
  state.appliedDeg10 = response.servo.appliedDeg10;
  state.timestampMs = response.timestampMs;

  if (!node.sendStateBinary(&state, sizeof(state))) {
    ESP_LOGW(logTag, "Failed sending servo ack state");
  }
}

bool sendModuleInfoNow(SlaveNode& node,
                       uint8_t index,
                       uint8_t total,
                       app::espnow::state_binary::ModuleDomain domain,
                       const char* id,
                       uint32_t featureBits,
                       const char* logTag) {
  app::espnow::state_binary::ModuleInfoState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::ModuleInfo);
  state.index = index;
  state.total = total;
  state.domain = static_cast<uint8_t>(domain);
  state.reserved0 = 0;
  state.featureBits = featureBits;
  if (id != nullptr) {
    strncpy(state.id, id, sizeof(state.id) - 1);
    state.id[sizeof(state.id) - 1] = '\0';
  }

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending module info state");
  }

  return sent;
}

bool sendModuleListNow(SlaveNode& node, const char* logTag) {
  app::sensing::SensorManager::ModuleDescriptor sensorDescriptors[app::sensing::SensorManager::MAX_MODULES] = {};
  app::actuation::ActuatorManager::ModuleDescriptor
      actuatorDescriptors[app::actuation::ActuatorManager::MAX_MODULES] = {};

  const size_t sensorCount =
      app::sensing::sensorManager.listModules(sensorDescriptors, app::sensing::SensorManager::MAX_MODULES);
  const size_t actuatorCount =
      app::actuation::actuatorManager.listModules(actuatorDescriptors, app::actuation::ActuatorManager::MAX_MODULES);
  const size_t totalCount = sensorCount + actuatorCount;
  const uint8_t total = totalCount > 255 ? 255 : static_cast<uint8_t>(totalCount);

  bool sentAny = false;
  uint8_t index = 0;
  for (size_t i = 0; i < sensorCount && index < total; ++i) {
    sentAny = sendModuleInfoNow(node,
                                index,
                                total,
                                app::espnow::state_binary::ModuleDomain::Sensor,
                                sensorDescriptors[i].id,
                                sensorDescriptors[i].featureBits,
                                logTag) || sentAny;
    index++;
  }

  for (size_t i = 0; i < actuatorCount && index < total; ++i) {
    sentAny = sendModuleInfoNow(node,
                                index,
                                total,
                                app::espnow::state_binary::ModuleDomain::Actuator,
                                actuatorDescriptors[i].id,
                                actuatorDescriptors[i].featureBits,
                                logTag) || sentAny;
    index++;
  }

  return sentAny;
}

bool handleCommandPacket(SlaveNode& node,
                         const uint8_t* payload,
                         size_t payloadSize,
                         const char* logTag) {
  if (payload == nullptr || payloadSize == 0) {
    ESP_LOGI(logTag, "Command packet received (empty)");
    return true;
  }

  if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                payloadSize,
                                                app::espnow::state_binary::Type::IdentityReq,
                                                sizeof(app::espnow::state_binary::IdentityReqCommand))) {
    sendIdentityStateNow(node, logTag);
    sendFeaturesStateNow(node, logTag);
    return true;
  }

  if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                payloadSize,
                                                app::espnow::state_binary::Type::WifiCredentials,
                                                sizeof(app::espnow::state_binary::WifiCredentialsCommand))) {
    const auto* command = reinterpret_cast<const app::espnow::state_binary::WifiCredentialsCommand*>(payload);
    char ssid[sizeof(command->ssid) + 1] = {0};
    char password[sizeof(command->password) + 1] = {0};
    memcpy(ssid, command->ssid, sizeof(command->ssid));
    memcpy(password, command->password, sizeof(command->password));

    if (!app::network::wifiManager.requestConnect(ssid, password)) {
      ESP_LOGW(logTag, "WiFi credentials command rejected");
    }
    return true;
  }

  if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                payloadSize,
                                                app::espnow::state_binary::Type::ServoControl,
                                                sizeof(app::espnow::state_binary::ServoControlCommand))) {
    const auto* command = reinterpret_cast<const app::espnow::state_binary::ServoControlCommand*>(payload);

    app::actuation::ActuationRequest request = {};
    request.kind = app::actuation::ActuatorKind::Servo;
    request.timestampMs = millis();
    strncpy(request.servo.group, command->group, sizeof(request.servo.group) - 1);
    request.servo.group[sizeof(request.servo.group) - 1] = '\0';
    request.servo.channel = command->channel;
    request.servo.targetDeg10 = command->targetDeg10;
    request.servo.transitionMs = command->transitionMs;

    app::actuation::ActuationResponse response = {};
    app::actuation::actuatorManager.handleRequest(request, response);
    sendServoAckNow(node, response, logTag);
    return true;
  }

  if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                payloadSize,
                                                app::espnow::state_binary::Type::ModuleListReq,
                                                sizeof(app::espnow::state_binary::ModuleListReqCommand))) {
    if (!sendModuleListNow(node, logTag)) {
      ESP_LOGW(logTag, "Module list request received but response send failed");
    }
    return true;
  }

  return false;
}

}  // namespace app::espnow::hooks
