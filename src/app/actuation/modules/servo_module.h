#pragma once

#include "app/actuation/actuator_module.h"

#include "app/actuator/servo_driver.h"
#include <app_config.h>

namespace app::actuation {

class ServoModule final : public IActuatorModule {
 public:
  const char* id() const override;
  uint32_t featureBit() const override;

  bool begin() override;
  void poll() override;
  bool handle(const ActuationRequest& request, ActuationResponse& response) override;

 private:
  bool groupMatches(const char* groupName) const;

  bool started = false;
  app::actuator::ServoDriver driver;
  uint16_t appliedDeg10[SERVO_GROUP_CHANNEL_COUNT] = {};
};

}  // namespace app::actuation
