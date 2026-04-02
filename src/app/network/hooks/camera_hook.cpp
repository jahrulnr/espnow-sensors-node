#include "camera_hook.h"

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
