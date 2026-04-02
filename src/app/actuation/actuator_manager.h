#pragma once

#include <Arduino.h>

#include "actuation_request.h"
#include "actuator_module.h"

namespace app::actuation {

class ActuatorManager {
 public:
  static constexpr size_t MAX_MODULES = 8;

  bool beginAll();
  void pollAll();
  bool handleRequest(const ActuationRequest& request, ActuationResponse& response);

  uint32_t featureBits();

 private:
  void ensureRegistryInitialized();
  bool registerModule(IActuatorModule& module);

  bool initialized = false;
  bool started = false;
  IActuatorModule* modules[MAX_MODULES] = {};
  size_t moduleCountValue = 0;
};

extern ActuatorManager actuatorManager;

}  // namespace app::actuation
