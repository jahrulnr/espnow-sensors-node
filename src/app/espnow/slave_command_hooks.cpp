#include "slave_command_hooks.h"

#include "slave.h"
#include "state_binary.h"

#include "app/actuation/actuator_manager.h"
#include "app/network/wifi_manager.h"
#include "app/security/wifi_secure_channel.h"
#include "app/sensor/camera_sensor.h"
#include "app/sensor/mmwave_sensor.h"
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

bool sendCameraCaptureStateNow(SlaveNode& node, const char* logTag) {
#if !CAMERA_SENSOR_ENABLED
  (void)node;
  (void)logTag;
  return true;
#else
  app::sensor::CameraSensor::CaptureMeta meta{};
  if (!app::sensor::cameraSensor.captureMeta(meta)) {
    ESP_LOGW(logTag, "Camera capture meta unavailable");
    return false;
  }

  app::espnow::state_binary::CameraCaptureState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::CameraCapture);
  state.width = meta.width;
  state.height = meta.height;
  state.frameBytes = meta.frameBytes;
  state.latencyMs = meta.latencyMs;
  state.frameFormat = meta.frameFormat;
  state.cameraType = meta.cameraType;

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending camera capture state");
  }
  return sent;
#endif
}

bool sendWifiKeyExchangeNow(SlaveNode& node, const char* logTag) {
  if (!app::security::wifiSecureChannel.begin()) {
    ESP_LOGW(logTag, "Secure channel unavailable");
    return false;
  }

  app::espnow::state_binary::WifiKeyExchangeState state = {};
  if (!app::security::wifiSecureChannel.fillKeyExchangeState(state)) {
    ESP_LOGW(logTag, "Failed preparing wifi key exchange state");
    return false;
  }

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending wifi key exchange state");
  }
  return sent;
}

bool sendWifiWsEndpointNow(SlaveNode& node, const char* logTag) {
  app::espnow::state_binary::WifiWsEndpointState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::WifiWsEndpoint);

  state.connected = app::network::wifiManager.isConnected() ? 1 : 0;
  state.port = static_cast<uint16_t>(WEBSOCKET_SERVER_PORT);

  const char* wsPath = WEBSOCKET_SERVER_PATH;
  if (wsPath != nullptr) {
    strncpy(state.path, wsPath, sizeof(state.path) - 1);
    state.path[sizeof(state.path) - 1] = '\0';
  }

  const char* hostname = app::network::wifiManager.getActiveHostname();
  if (hostname != nullptr) {
    strncpy(state.hostname, hostname, sizeof(state.hostname) - 1);
    state.hostname[sizeof(state.hostname) - 1] = '\0';
  }

  if (!app::network::wifiManager.getLocalIpBytes(state.ip)) {
    memset(state.ip, 0, sizeof(state.ip));
  }

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW(logTag, "Failed sending WiFi websocket endpoint state");
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
    if (!sendModuleInfoNow(node,
                           index,
                           total,
                           app::espnow::state_binary::ModuleDomain::Sensor,
                           sensorDescriptors[i].id,
                           sensorDescriptors[i].featureBits,
                           logTag)) {
      return sentAny;
    }
    sentAny = true;
    index++;
  }

  for (size_t i = 0; i < actuatorCount && index < total; ++i) {
    if (!sendModuleInfoNow(node,
                           index,
                           total,
                           app::espnow::state_binary::ModuleDomain::Actuator,
                           actuatorDescriptors[i].id,
                           actuatorDescriptors[i].featureBits,
                           logTag)) {
      return sentAny;
    }
    sentAny = true;
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
    sendCameraCaptureStateNow(node, logTag);
    sendWifiKeyExchangeNow(node, logTag);
    sendWifiWsEndpointNow(node, logTag);
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
                                                app::espnow::state_binary::Type::WifiCredentialsSecure,
                                                sizeof(app::espnow::state_binary::WifiCredentialsSecureCommand))) {
    if (!app::security::wifiSecureChannel.begin()) {
      ESP_LOGW(logTag, "Secure channel unavailable for WifiCredentialsSecure");
      return true;
    }

    const auto* command = reinterpret_cast<const app::espnow::state_binary::WifiCredentialsSecureCommand*>(payload);
    char ssid[33] = {0};
    char password[65] = {0};
    if (!app::security::wifiSecureChannel.decryptCredentials(*command,
                                                             ssid,
                                                             sizeof(ssid),
                                                             password,
                                                             sizeof(password))) {
      ESP_LOGW(logTag, "Secure WiFi credentials command rejected");
      return true;
    }

    if (!app::network::wifiManager.requestConnect(ssid, password)) {
      ESP_LOGW(logTag, "Secure WiFi credentials connect request rejected");
    }

    std::memset(password, 0, sizeof(password));
    return true;
  }

  if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                payloadSize,
                                                app::espnow::state_binary::Type::MmwaveRangeConfig,
                                                sizeof(app::espnow::state_binary::MmwaveRangeConfigCommand))) {
    const auto* command = reinterpret_cast<const app::espnow::state_binary::MmwaveRangeConfigCommand*>(payload);
    const bool persist = command->persistToNvs != 0;
    const bool stored = app::sensor::mmwaveSensor.setMaxDetectionRangeCm(command->maxDistanceCm, persist);
    if (persist && !stored) {
      ESP_LOGW(logTag,
               "MmwaveRangeConfig apply failed to persist maxDistanceCm=%u",
               static_cast<unsigned>(command->maxDistanceCm));
      return true;
    }

    ESP_LOGI(logTag,
             "MmwaveRangeConfig applied maxDistanceCm=%u persist=%u",
             static_cast<unsigned>(command->maxDistanceCm),
             persist ? 1U : 0U);
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
