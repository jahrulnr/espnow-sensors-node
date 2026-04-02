#include "actuator_manager.h"

#include "modules/servo_module.h"

namespace app::actuation {

namespace {

ServoModule servoModule;

}  // namespace

ActuatorManager actuatorManager;

bool ActuatorManager::registerModule(IActuatorModule& module) {
  if (moduleCountValue >= MAX_MODULES) {
    return false;
  }

  modules[moduleCountValue++] = &module;
  return true;
}

void ActuatorManager::ensureRegistryInitialized() {
  if (!initialized) {
    registerModule(servoModule);
    initialized = true;
  }
}

bool ActuatorManager::beginAll() {
  ensureRegistryInitialized();

  if (started) {
    return true;
  }

  bool ok = true;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    ok = modules[i]->begin() && ok;
  }

  started = true;
  return ok;
}

void ActuatorManager::pollAll() {
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    modules[i]->poll();
  }
}

bool ActuatorManager::handleRequest(const ActuationRequest& request, ActuationResponse& response) {
  ensureRegistryInitialized();

  if (!started) {
    beginAll();
  }

  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    if (modules[i]->handle(request, response)) {
      return true;
    }
  }

  response.ok = false;
  response.status = ActuationStatus::DriverError;
  response.timestampMs = millis();
  return false;
}

uint32_t ActuatorManager::featureBits() {
  ensureRegistryInitialized();

  uint32_t bits = 0;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    bits |= modules[i]->featureBit();
  }

  return bits;
}

}  // namespace app::actuation
