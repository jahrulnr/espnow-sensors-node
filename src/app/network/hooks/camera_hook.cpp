#include "camera_hook.h"

#include "app/power/sleep_guard.h"
#include "app/network/websocket_gateway.h"
#include "app/sensor/camera_sensor.h"

#include <SpiJsonDocument.h>

#if CAMERA_SENSOR_ENABLED
#include <esp_camera.h>
#endif

namespace app::network {

namespace {

bool strEq(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return strcmp(a, b) == 0;
}

}  // namespace

const char* CameraHook::type() const {
  return "camera";
}

bool CameraHook::begin(WebsocketGateway& gateway) {
  (void)gateway;
  return true;
}

void CameraHook::loop(WebsocketGateway& gateway) {
  runStreams(gateway);
}

bool CameraHook::handleRequest(WebsocketGateway& gateway, uint8_t clientId, ArduinoJson::JsonVariantConst requestData) {
  const char* state = requestData["state"].as<const char*>();
  if (state == nullptr || state[0] == '\0') {
    state = "specs";
  }

#if !CAMERA_SENSOR_ENABLED
  if (strEq(state, "specs")) {
    return sendCameraSpecs(gateway, clientId);
  }

  if (strEq(state, "config_get")) {
    return gateway.sendError(clientId,
                             type(),
                             "camera_disabled",
                             "Camera is not enabled in this build profile");
  }

  if (strEq(state, "config_set")) {
    return gateway.sendError(clientId,
                             type(),
                             "camera_disabled",
                             "Camera is not enabled in this build profile");
  }

  return gateway.sendError(clientId,
                           type(),
                           "camera_disabled",
                           "Camera is not enabled in this build profile");
#else
  if (strEq(state, "specs")) {
    return sendCameraSpecs(gateway, clientId);
  }

  if (strEq(state, "frame")) {
    return captureAndSendFrame(gateway, clientId, "single");
  }

  if (strEq(state, "config_get")) {
    return sendCameraConfig(gateway, clientId, "config_get");
  }

  if (strEq(state, "config_set")) {
    return handleCameraConfigSet(gateway, clientId, requestData);
  }

  if (strEq(state, "stream")) {
    bool enabled = true;
    const char* action = requestData["action"].as<const char*>();
    if (strEq(action, "stop")) {
      enabled = false;
    } else if (strEq(action, "start") || action == nullptr || action[0] == '\0') {
      enabled = true;
    } else {
      return gateway.sendError(clientId, type(), "invalid_action", "Action must be 'start' or 'stop'");
    }

    uint32_t intervalMs = requestData["intervalMs"].is<uint32_t>()
                              ? requestData["intervalMs"].as<uint32_t>()
                              : static_cast<uint32_t>(WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS);

    if (!setCameraStream(gateway, clientId, enabled, intervalMs)) {
      return gateway.sendError(clientId,
                               type(),
                               "stream_update_failed",
                               "Failed to update camera stream for this client");
    }

    if (enabled && requestData["sendFirstFrame"].is<bool>() && requestData["sendFirstFrame"].as<bool>()) {
      captureAndSendFrame(gateway, clientId, "stream");
    }

    SpiJsonDocument data;
    data["state"] = "stream";
    data["status"] = enabled ? "started" : "stopped";
    data["intervalMs"] = intervalMs < WEBSOCKET_STREAM_MIN_INTERVAL_MS
                              ? static_cast<uint32_t>(WEBSOCKET_STREAM_MIN_INTERVAL_MS)
                              : intervalMs;
    data["format"] = WEBSOCKET_CAMERA_DEFAULT_FORMAT;
    return gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
  }

  return gateway.sendError(clientId, type(), "unknown_state", "Unknown camera state request");
#endif
}

void CameraHook::onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) {
  (void)gateway;
  StreamClient* streamClient = findStreamClient(clientId);
  if (streamClient != nullptr) {
    streamClient->active = false;
    refreshStreamSleepHold();
  }
}

void CameraHook::refreshStreamSleepHold() {
  bool hasActiveStreams = false;
  for (size_t i = 0; i < WEBSOCKET_STREAM_MAX_CLIENTS; ++i) {
    if (streamClients[i].active) {
      hasActiveStreams = true;
      break;
    }
  }

  if (hasActiveStreams && !streamSleepHold) {
    app::power::beginMasterTask();
    streamSleepHold = true;
  } else if (!hasActiveStreams && streamSleepHold) {
    app::power::endMasterTask();
    streamSleepHold = false;
  }

  if (hasActiveStreams) {
    app::power::touchMasterActivity();
  }
}

CameraHook::StreamClient* CameraHook::findStreamClient(uint8_t clientId) {
  for (size_t i = 0; i < WEBSOCKET_STREAM_MAX_CLIENTS; ++i) {
    if (streamClients[i].active && streamClients[i].clientId == clientId) {
      return &streamClients[i];
    }
  }

  return nullptr;
}

CameraHook::StreamClient* CameraHook::ensureStreamClient(uint8_t clientId) {
  StreamClient* existing = findStreamClient(clientId);
  if (existing != nullptr) {
    return existing;
  }

  for (size_t i = 0; i < WEBSOCKET_STREAM_MAX_CLIENTS; ++i) {
    if (streamClients[i].active) {
      continue;
    }

    streamClients[i].active = true;
    streamClients[i].clientId = clientId;
    streamClients[i].intervalMs = WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS;
    streamClients[i].nextDueMs = millis();
    return &streamClients[i];
  }

  return nullptr;
}

bool CameraHook::ensureCameraReady() {
#if !CAMERA_SENSOR_ENABLED
  return false;
#else
  return app::sensor::cameraSensor.begin();
#endif
}

bool CameraHook::sendCameraSpecs(WebsocketGateway& gateway, uint8_t clientId) {
  SpiJsonDocument data;
  data["state"] = "specs";
  data["format"] = WEBSOCKET_CAMERA_DEFAULT_FORMAT;
  data["width"] = WEBSOCKET_CAMERA_DEFAULT_WIDTH;
  data["height"] = WEBSOCKET_CAMERA_DEFAULT_HEIGHT;
  data["streamIntervalDefaultMs"] = WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS;
  data["streamIntervalMinMs"] = WEBSOCKET_STREAM_MIN_INTERVAL_MS;

#if !CAMERA_SENSOR_ENABLED
  data["enabled"] = false;
  data["ready"] = false;
#else
  data["enabled"] = true;
  data["ready"] = ensureCameraReady();
#endif

  return gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
}

bool CameraHook::sendCameraConfig(WebsocketGateway& gateway, uint8_t clientId, const char* state) {
#if !CAMERA_SENSOR_ENABLED
  (void)gateway;
  (void)clientId;
  (void)state;
  return false;
#else
  if (!ensureCameraReady()) {
    return gateway.sendError(clientId, type(), "camera_init_failed", "Camera initialization failed");
  }

  app::sensor::CameraSensor::RuntimeConfig cfg;
  if (!app::sensor::cameraSensor.getRuntimeConfig(cfg)) {
    return gateway.sendError(clientId, type(), "camera_status_failed", "Failed to read camera runtime config");
  }

  SpiJsonDocument data;
  data["state"] = state == nullptr ? "config_get" : state;
  data["ready"] = cfg.ready;
  data["frameSize"] = cfg.frameSize;
  data["quality"] = cfg.quality;
  data["brightness"] = cfg.brightness;
  data["contrast"] = cfg.contrast;
  data["saturation"] = cfg.saturation;
  data["hmirror"] = cfg.hmirror;
  data["vflip"] = cfg.vflip;
  data["xclkHz"] = cfg.xclkHz;

  return gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
#endif
}

bool CameraHook::handleCameraConfigSet(WebsocketGateway& gateway,
                                       uint8_t clientId,
                                       ArduinoJson::JsonVariantConst requestData) {
#if !CAMERA_SENSOR_ENABLED
  (void)gateway;
  (void)clientId;
  (void)requestData;
  return false;
#else
  if (!ensureCameraReady()) {
    return gateway.sendError(clientId, type(), "camera_init_failed", "Camera initialization failed");
  }

  bool changed = false;

  if (!requestData["frameSize"].isNull()) {
    if (!requestData["frameSize"].is<uint8_t>()) {
      return gateway.sendError(clientId, type(), "invalid_framesize", "frameSize must be uint8");
    }
    const uint8_t frameSize = requestData["frameSize"].as<uint8_t>();
    if (frameSize >= static_cast<uint8_t>(FRAMESIZE_INVALID)) {
      return gateway.sendError(clientId, type(), "invalid_framesize", "frameSize out of range");
    }
    if (!app::sensor::cameraSensor.setFrameSize(frameSize)) {
      return gateway.sendError(clientId, type(), "set_framesize_failed", "Failed to apply frameSize");
    }
    changed = true;
  }

  if (!requestData["xclkHz"].isNull()) {
    if (!requestData["xclkHz"].is<int>()) {
      return gateway.sendError(clientId, type(), "invalid_xclk", "xclkHz must be integer");
    }
    const int xclkHz = requestData["xclkHz"].as<int>();
    if (xclkHz < 1000000 || xclkHz > 30000000) {
      return gateway.sendError(clientId, type(), "invalid_xclk", "xclkHz must be between 1000000 and 30000000");
    }
    if (!app::sensor::cameraSensor.setXclkHz(xclkHz)) {
      return gateway.sendError(clientId, type(), "set_xclk_failed", "Failed to apply xclkHz");
    }
    changed = true;
  }

  if (!requestData["hmirror"].isNull()) {
    if (!requestData["hmirror"].is<bool>()) {
      return gateway.sendError(clientId, type(), "invalid_hmirror", "hmirror must be boolean");
    }
    if (!app::sensor::cameraSensor.setHmirror(requestData["hmirror"].as<bool>())) {
      return gateway.sendError(clientId, type(), "set_hmirror_failed", "Failed to apply hmirror");
    }
    changed = true;
  }

  if (!requestData["vflip"].isNull()) {
    if (!requestData["vflip"].is<bool>()) {
      return gateway.sendError(clientId, type(), "invalid_vflip", "vflip must be boolean");
    }
    if (!app::sensor::cameraSensor.setVflip(requestData["vflip"].as<bool>())) {
      return gateway.sendError(clientId, type(), "set_vflip_failed", "Failed to apply vflip");
    }
    changed = true;
  }

  if (!requestData["quality"].isNull()) {
    if (!requestData["quality"].is<uint8_t>()) {
      return gateway.sendError(clientId, type(), "invalid_quality", "quality must be uint8");
    }
    const uint8_t quality = requestData["quality"].as<uint8_t>();
    if (quality > 63) {
      return gateway.sendError(clientId, type(), "invalid_quality", "quality must be 0..63");
    }
    if (!app::sensor::cameraSensor.setQuality(quality)) {
      return gateway.sendError(clientId, type(), "set_quality_failed", "Failed to apply quality");
    }
    changed = true;
  }

  if (!requestData["brightness"].isNull()) {
    if (!requestData["brightness"].is<int8_t>() && !requestData["brightness"].is<int>()) {
      return gateway.sendError(clientId, type(), "invalid_brightness", "brightness must be integer");
    }
    const int brightness = requestData["brightness"].as<int>();
    if (brightness < -2 || brightness > 2) {
      return gateway.sendError(clientId, type(), "invalid_brightness", "brightness must be -2..2");
    }
    if (!app::sensor::cameraSensor.setBrightness(static_cast<int8_t>(brightness))) {
      return gateway.sendError(clientId, type(), "set_brightness_failed", "Failed to apply brightness");
    }
    changed = true;
  }

  if (!requestData["contrast"].isNull()) {
    if (!requestData["contrast"].is<int8_t>() && !requestData["contrast"].is<int>()) {
      return gateway.sendError(clientId, type(), "invalid_contrast", "contrast must be integer");
    }
    const int contrast = requestData["contrast"].as<int>();
    if (contrast < -2 || contrast > 2) {
      return gateway.sendError(clientId, type(), "invalid_contrast", "contrast must be -2..2");
    }
    if (!app::sensor::cameraSensor.setContrast(static_cast<int8_t>(contrast))) {
      return gateway.sendError(clientId, type(), "set_contrast_failed", "Failed to apply contrast");
    }
    changed = true;
  }

  if (!requestData["saturation"].isNull()) {
    if (!requestData["saturation"].is<int8_t>() && !requestData["saturation"].is<int>()) {
      return gateway.sendError(clientId, type(), "invalid_saturation", "saturation must be integer");
    }
    const int saturation = requestData["saturation"].as<int>();
    if (saturation < -2 || saturation > 2) {
      return gateway.sendError(clientId, type(), "invalid_saturation", "saturation must be -2..2");
    }
    if (!app::sensor::cameraSensor.setSaturation(static_cast<int8_t>(saturation))) {
      return gateway.sendError(clientId, type(), "set_saturation_failed", "Failed to apply saturation");
    }
    changed = true;
  }

  if (!changed) {
    return gateway.sendError(clientId,
                             type(),
                             "invalid_config_request",
                             "Provide at least one field: frameSize, xclkHz, hmirror, vflip, quality, brightness, contrast, saturation");
  }

  return sendCameraConfig(gateway, clientId, "config_set");
#endif
}

bool CameraHook::captureAndSendFrame(WebsocketGateway& gateway, uint8_t clientId, const char* mode) {
#if !CAMERA_SENSOR_ENABLED
  (void)gateway;
  (void)clientId;
  (void)mode;
  return false;
#else
  if (!ensureCameraReady()) {
    return gateway.sendError(clientId, type(), "camera_init_failed", "Camera initialization failed");
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
    return gateway.sendError(clientId, type(), "capture_failed", "Failed to capture camera frame");
  }

  SpiJsonDocument data;
  data["state"] = "frame";
  data["mode"] = mode == nullptr ? "single" : mode;
  data["sequence"] = ++cameraSequence;
  data["format"] = WEBSOCKET_CAMERA_DEFAULT_FORMAT;
  data["binary"] = true;
  data["bytes"] = static_cast<uint32_t>(fb->len);
  data["width"] = static_cast<uint32_t>(fb->width);
  data["height"] = static_cast<uint32_t>(fb->height);

  const bool metadataSent = gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
  const bool binarySent = metadataSent && gateway.sendBinary(clientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  return binarySent;
#endif
}

bool CameraHook::setCameraStream(WebsocketGateway& gateway, uint8_t clientId, bool enabled, uint32_t intervalMs) {
  (void)gateway;
#if !CAMERA_SENSOR_ENABLED
  (void)clientId;
  (void)enabled;
  (void)intervalMs;
  return false;
#else
  if (!enabled) {
    StreamClient* streamClient = findStreamClient(clientId);
    if (streamClient != nullptr) {
      streamClient->active = false;
    }
    refreshStreamSleepHold();
    return true;
  }

  if (intervalMs < WEBSOCKET_STREAM_MIN_INTERVAL_MS) {
    intervalMs = WEBSOCKET_STREAM_MIN_INTERVAL_MS;
  }

  if (!ensureCameraReady()) {
    return false;
  }

  StreamClient* streamClient = ensureStreamClient(clientId);
  if (streamClient == nullptr) {
    return false;
  }

  streamClient->active = true;
  streamClient->intervalMs = intervalMs;
  streamClient->nextDueMs = millis();
  refreshStreamSleepHold();
  return true;
#endif
}

void CameraHook::runStreams(WebsocketGateway& gateway) {
#if !CAMERA_SENSOR_ENABLED
  (void)gateway;
  return;
#else
  size_t dueCount = 0;
  size_t dueIndexes[WEBSOCKET_STREAM_MAX_CLIENTS] = {0};
  const uint32_t now = millis();

  for (size_t i = 0; i < WEBSOCKET_STREAM_MAX_CLIENTS; ++i) {
    if (!streamClients[i].active) {
      continue;
    }

    if (now >= streamClients[i].nextDueMs) {
      dueIndexes[dueCount++] = i;
    }
  }

  if (dueCount == 0) {
    return;
  }

  app::power::touchMasterActivity();

  if (!ensureCameraReady()) {
    for (size_t i = 0; i < dueCount; ++i) {
      StreamClient& streamClient = streamClients[dueIndexes[i]];
      gateway.sendError(streamClient.clientId, type(), "camera_init_failed", "Camera initialization failed");
      streamClient.nextDueMs = now + streamClient.intervalMs;
    }
    return;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
    for (size_t i = 0; i < dueCount; ++i) {
      StreamClient& streamClient = streamClients[dueIndexes[i]];
      gateway.sendError(streamClient.clientId, type(), "capture_failed", "Failed to capture camera frame");
      streamClient.nextDueMs = now + streamClient.intervalMs;
    }
    return;
  }

  const uint32_t sequence = ++cameraSequence;
  for (size_t i = 0; i < dueCount; ++i) {
    StreamClient& streamClient = streamClients[dueIndexes[i]];

    SpiJsonDocument data;
    data["state"] = "frame";
    data["mode"] = "stream";
    data["sequence"] = sequence;
    data["format"] = WEBSOCKET_CAMERA_DEFAULT_FORMAT;
    data["binary"] = true;
    data["bytes"] = static_cast<uint32_t>(fb->len);
    data["width"] = static_cast<uint32_t>(fb->width);
    data["height"] = static_cast<uint32_t>(fb->height);

    const bool metadataSent = gateway.sendEnvelope(streamClient.clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
    if (metadataSent) {
      gateway.sendBinary(streamClient.clientId, fb->buf, fb->len);
    }

    streamClient.nextDueMs = now + streamClient.intervalMs;
  }

  esp_camera_fb_return(fb);
#endif
}

}  // namespace app::network
