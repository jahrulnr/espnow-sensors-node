#pragma once

#include <app_config.h>

#include "websocket_hook.h"

namespace app::network {

class CameraHook : public IWebsocketHook {
 public:
  const char* type() const override;
  bool begin(WebsocketGateway& gateway) override;
  void loop(WebsocketGateway& gateway) override;
  bool handleRequest(WebsocketGateway& gateway, uint8_t clientId, ArduinoJson::JsonVariantConst requestData) override;
  void onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) override;

 private:
  struct StreamClient {
    bool active = false;
    uint8_t clientId = 0;
    uint32_t intervalMs = WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS;
    uint32_t nextDueMs = 0;
  };

  StreamClient streamClients[WEBSOCKET_STREAM_MAX_CLIENTS] = {};
  uint32_t cameraSequence = 0;
  bool cameraInitAttempted = false;
  bool cameraReady = false;

  StreamClient* findStreamClient(uint8_t clientId);
  StreamClient* ensureStreamClient(uint8_t clientId);
  bool ensureCameraReady();
  bool sendCameraSpecs(WebsocketGateway& gateway, uint8_t clientId);
  bool captureAndSendFrame(WebsocketGateway& gateway, uint8_t clientId, const char* mode);
  bool setCameraStream(WebsocketGateway& gateway, uint8_t clientId, bool enabled, uint32_t intervalMs);
  void runStreams(WebsocketGateway& gateway);
};

}  // namespace app::network
