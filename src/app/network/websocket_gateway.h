#pragma once

#include <app_config.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "hooks/websocket_hook_manager.h"

class WebSocketsServer;

namespace app::network {

class WebsocketGateway {
 public:
  WebsocketGateway() = default;

  void onWifiConnectionChanged(bool connected);
  void loop();

  bool sendEnvelope(uint8_t clientId, const char* type, ArduinoJson::JsonVariantConst data);
  bool sendError(uint8_t clientId, const char* type, const char* code, const char* message);
  bool sendBinary(uint8_t clientId, const uint8_t* payload, size_t payloadSize);
  void fillRegisteredHooks(ArduinoJson::JsonArray out) const;

  static void onEventStatic(uint8_t clientId, uint8_t eventType, uint8_t* payload, size_t length);

 private:
  bool started = false;
  bool wifiConnected = false;
  bool hooksStarted = false;
  WebsocketHookManager hookManager;

  WebSocketsServer* server = nullptr;

  static WebsocketGateway* activeInstance;

  bool handleTextMessage(uint8_t clientId, const uint8_t* payload, size_t length);
  void onEvent(uint8_t clientId, uint8_t eventType, uint8_t* payload, size_t length);
  void ensureServerStarted();
};

extern WebsocketGateway websocketGateway;

}  // namespace app::network
