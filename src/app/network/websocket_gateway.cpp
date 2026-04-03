#include "websocket_gateway.h"

#include "app/power/sleep_guard.h"

#include <SpiJsonDocument.h>
#include <WebSocketsServer.h>
#include <esp_log.h>

namespace app::network {

namespace {

static constexpr const char* TAG = "WS_GATEWAY";

void websocketEventThunk(uint8_t clientId, WStype_t eventType, uint8_t* payload, size_t length) {
  WebsocketGateway::onEventStatic(clientId, static_cast<uint8_t>(eventType), payload, length);
}

}  // namespace

WebsocketGateway* WebsocketGateway::activeInstance = nullptr;
WebsocketGateway websocketGateway;

void WebsocketGateway::onWifiConnectionChanged(bool connected) {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  wifiConnected = connected;
  if (wifiConnected) {
    ensureServerStarted();
    if (!hooksStarted) {
      hooksStarted = hookManager.beginAll(*this);
      if (!hooksStarted) {
        ESP_LOGW(TAG, "Some websocket hooks failed to begin");
      }
    }
  } else {
    hooksStarted = false;
  }
#else
  (void)connected;
#endif
}

void WebsocketGateway::loop() {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  if (!started || !wifiConnected || server == nullptr) {
    return;
  }

  server->loop();

  if (!hooksStarted) {
    hooksStarted = hookManager.beginAll(*this);
  }
  hookManager.loopAll(*this);
#endif
}

bool WebsocketGateway::sendEnvelope(uint8_t clientId, const char* type, ArduinoJson::JsonVariantConst data) {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  if (server == nullptr || type == nullptr || type[0] == '\0') {
    return false;
  }

  SpiJsonDocument envelope;
  envelope["type"] = type;
  envelope["data"].set(data);

  String out;
  serializeJson(envelope, out);
  return server->sendTXT(clientId, out);
#else
  (void)clientId;
  (void)type;
  (void)data;
  return false;
#endif
}

bool WebsocketGateway::sendError(uint8_t clientId, const char* type, const char* code, const char* message) {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  SpiJsonDocument data;
  data["ok"] = false;
  data["error"]["code"] = code == nullptr ? "unknown" : code;
  data["error"]["message"] = message == nullptr ? "unknown error" : message;

  return sendEnvelope(clientId, type == nullptr ? "system" : type, data.as<ArduinoJson::JsonVariantConst>());
#else
  (void)clientId;
  (void)type;
  (void)code;
  (void)message;
  return false;
#endif
}

bool WebsocketGateway::sendBinary(uint8_t clientId, const uint8_t* payload, size_t payloadSize) {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  if (server == nullptr || payload == nullptr || payloadSize == 0) {
    return false;
  }

  return server->sendBIN(clientId, payload, payloadSize);
#else
  (void)clientId;
  (void)payload;
  (void)payloadSize;
  return false;
#endif
}

void WebsocketGateway::fillRegisteredHooks(ArduinoJson::JsonArray out) const {
  if (out.isNull()) {
    return;
  }

  for (size_t i = 0; i < hookManager.hookCount(); ++i) {
    const char* type = hookManager.hookTypeAt(i);
    if (type != nullptr && type[0] != '\0') {
      out.add(type);
    }
  }
}

void WebsocketGateway::onEventStatic(uint8_t clientId, uint8_t eventType, uint8_t* payload, size_t length) {
#if ENABLE_WIFI_MODE && ENABLE_WEBSOCKET_GATEWAY
  if (activeInstance != nullptr) {
    activeInstance->onEvent(clientId, eventType, payload, length);
  }
#else
  (void)clientId;
  (void)eventType;
  (void)payload;
  (void)length;
#endif
}


bool WebsocketGateway::handleTextMessage(uint8_t clientId, const uint8_t* payload, size_t length) {
  app::power::touchMasterActivity();

  if (payload == nullptr || length == 0) {
    return sendError(clientId, "system", "empty_request", "Empty websocket text payload");
  }

  SpiJsonDocument request;
  const auto err = deserializeJson(request, payload, length);
  if (err != DeserializationError::Ok) {
    return sendError(clientId, "system", "invalid_json", err.c_str());
  }

  const char* type = request["type"].as<const char*>();
  if (type == nullptr || type[0] == '\0') {
    return sendError(clientId, "system", "missing_type", "Field 'type' is required");
  }

  const auto result = hookManager.dispatch(*this, type, clientId, request["data"]);
  if (result == WebsocketHookManager::DispatchResult::NotFound) {
    return sendError(clientId, type, "unsupported_type", "No hook registered for request type");
  }
  if (result == WebsocketHookManager::DispatchResult::Failed) {
    return sendError(clientId, type, "handler_failed", "Request hook failed");
  }

  return true;
}

void WebsocketGateway::onEvent(uint8_t clientId, uint8_t eventType, uint8_t* payload, size_t length) {
  const WStype_t type = static_cast<WStype_t>(eventType);
  switch (type) {
    case WStype_CONNECTED:
      app::power::touchMasterActivity();
      ESP_LOGI(TAG, "Client %u connected", static_cast<unsigned>(clientId));
      break;
    case WStype_DISCONNECTED:
      app::power::touchMasterActivity();
      ESP_LOGI(TAG, "Client %u disconnected", static_cast<unsigned>(clientId));
      hookManager.onClientDisconnected(*this, clientId);
      break;
    case WStype_TEXT:
      handleTextMessage(clientId, payload, length);
      break;
    case WStype_BIN:
      app::power::touchMasterActivity();
      sendError(clientId, "system", "unsupported_payload", "Binary request payload is not supported");
      break;
    default:
      break;
  }
}

void WebsocketGateway::ensureServerStarted() {
  if (started) {
    return;
  }

  server = new WebSocketsServer(WEBSOCKET_SERVER_PORT);
  if (server == nullptr) {
    ESP_LOGE(TAG, "Failed allocating WebSocketsServer");
    return;
  }

  activeInstance = this;
  server->begin();
  server->onEvent(websocketEventThunk);
  server->enableHeartbeat(15000, 3000, 2);
  started = true;

  ESP_LOGI(TAG, "WebSocket gateway active on port %u", static_cast<unsigned>(WEBSOCKET_SERVER_PORT));
}

}  // namespace app::network
