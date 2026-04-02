#pragma once

#include "websocket_hook.h"

namespace app::network {

class SystemHook : public IWebsocketHook {
 public:
  const char* type() const override;
  bool begin(WebsocketGateway& gateway) override;
  void loop(WebsocketGateway& gateway) override;
  bool handleRequest(WebsocketGateway& gateway, uint8_t clientId, ArduinoJson::JsonVariantConst requestData) override;
  void onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) override;
};

}  // namespace app::network
