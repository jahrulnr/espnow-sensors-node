#pragma once

#include <Arduino.h>

#include "actuation_request.h"

namespace app::actuation {

class IActuatorModule {
 public:
  virtual ~IActuatorModule() = default;

  virtual const char* id() const = 0;
  virtual uint32_t featureBit() const = 0;

  virtual bool begin() = 0;
  virtual void poll() = 0;
  virtual bool handle(const ActuationRequest& request, ActuationResponse& response) = 0;
};

}  // namespace app::actuation
