#pragma once

#include <app_config.h>

#include "websocket_hook.h"

namespace app::network {

class WebsocketGateway;

class WebsocketHookManager {
 public:
  enum class DispatchResult : uint8_t {
    Handled,
    NotFound,
    Failed,
  };

  static constexpr size_t MAX_HOOKS = WEBSOCKET_MAX_HOOKS;

  bool beginAll(WebsocketGateway& gateway);
  void loopAll(WebsocketGateway& gateway);
  DispatchResult dispatch(WebsocketGateway& gateway,
                          const char* type,
                          uint8_t clientId,
                          ArduinoJson::JsonVariantConst requestData);
  void onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId);

  size_t hookCount() const { return hookCountValue; }
  const char* hookTypeAt(size_t index) const;

 private:
  bool registerHook(IWebsocketHook& hook);
  void ensureRegistryInitialized();

  bool initialized = false;
  bool started = false;
  IWebsocketHook* hooks[MAX_HOOKS] = {};
  size_t hookCountValue = 0;
};

}  // namespace app::network
