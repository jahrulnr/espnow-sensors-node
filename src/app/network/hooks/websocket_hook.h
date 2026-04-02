#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace app::network {

class WebsocketGateway;

class IWebsocketHook {
 public:
  virtual ~IWebsocketHook() = default;

  virtual const char* type() const = 0;
  virtual bool begin(WebsocketGateway& gateway) = 0;
  virtual void loop(WebsocketGateway& gateway) = 0;
  virtual bool handleRequest(WebsocketGateway& gateway, uint8_t clientId, ArduinoJson::JsonVariantConst requestData) = 0;
  virtual void onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) = 0;
};

}  // namespace app::network
