#include "system_hook.h"

#include "app/network/websocket_gateway.h"

#include <SpiJsonDocument.h>

namespace app::network {

const char* SystemHook::type() const {
  return "system";
}

bool SystemHook::begin(WebsocketGateway& gateway) {
  (void)gateway;
  return true;
}

void SystemHook::loop(WebsocketGateway& gateway) {
  (void)gateway;
}

bool SystemHook::handleRequest(WebsocketGateway& gateway, uint8_t clientId, ArduinoJson::JsonVariantConst requestData) {
  const char* state = requestData["state"].as<const char*>();
  if (state == nullptr || state[0] == '\0') {
    state = "ping";
  }

  SpiJsonDocument data;
  if (strcmp(state, "hooks") == 0) {
    data["state"] = "hooks";
    data["transport"] = "websocket";
    ArduinoJson::JsonArray items = data["items"].to<ArduinoJson::JsonArray>();
    gateway.fillRegisteredHooks(items);
    return gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
  }

  data["state"] = "pong";
  data["transport"] = "websocket";
  return gateway.sendEnvelope(clientId, type(), data.as<ArduinoJson::JsonVariantConst>());
}

void SystemHook::onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) {
  (void)gateway;
  (void)clientId;
}

}  // namespace app::network
