#include "websocket_hook_manager.h"

#include "camera_hook.h"
#include "system_hook.h"

namespace app::network {

namespace {

SystemHook systemHook;
CameraHook cameraHook;

}  // namespace

bool WebsocketHookManager::registerHook(IWebsocketHook& hook) {
  if (hookCountValue >= MAX_HOOKS) {
    return false;
  }

  hooks[hookCountValue++] = &hook;
  return true;
}

void WebsocketHookManager::ensureRegistryInitialized() {
  if (!initialized) {
    registerHook(systemHook);
    registerHook(cameraHook);
    initialized = true;
  }
}

bool WebsocketHookManager::beginAll(WebsocketGateway& gateway) {
  ensureRegistryInitialized();

  if (started) {
    return true;
  }

  bool ok = true;
  for (size_t i = 0; i < hookCountValue; ++i) {
    if (hooks[i] == nullptr) {
      continue;
    }

    ok = hooks[i]->begin(gateway) && ok;
  }

  started = true;
  return ok;
}

void WebsocketHookManager::loopAll(WebsocketGateway& gateway) {
  for (size_t i = 0; i < hookCountValue; ++i) {
    if (hooks[i] == nullptr) {
      continue;
    }

    hooks[i]->loop(gateway);
  }
}

WebsocketHookManager::DispatchResult WebsocketHookManager::dispatch(WebsocketGateway& gateway,
                                                                    const char* type,
                                                                    uint8_t clientId,
                                                                    ArduinoJson::JsonVariantConst requestData) {
  ensureRegistryInitialized();

  if (type == nullptr || type[0] == '\0') {
    return DispatchResult::NotFound;
  }

  for (size_t i = 0; i < hookCountValue; ++i) {
    if (hooks[i] == nullptr) {
      continue;
    }

    const char* hookType = hooks[i]->type();
    if (hookType == nullptr || strcmp(hookType, type) != 0) {
      continue;
    }

    return hooks[i]->handleRequest(gateway, clientId, requestData)
               ? DispatchResult::Handled
               : DispatchResult::Failed;
  }

  return DispatchResult::NotFound;
}

void WebsocketHookManager::onClientDisconnected(WebsocketGateway& gateway, uint8_t clientId) {
  for (size_t i = 0; i < hookCountValue; ++i) {
    if (hooks[i] == nullptr) {
      continue;
    }

    hooks[i]->onClientDisconnected(gateway, clientId);
  }
}

const char* WebsocketHookManager::hookTypeAt(size_t index) const {
  if (index >= hookCountValue || hooks[index] == nullptr) {
    return nullptr;
  }

  return hooks[index]->type();
}

}  // namespace app::network
