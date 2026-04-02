#pragma once

#include <Arduino.h>

namespace app::sensor {

class CameraSensor {
 public:
  CameraSensor() = default;

  bool begin();
  bool isReady() const { return ready; }

 private:
  bool initAttempted = false;
  bool ready = false;
};

extern CameraSensor cameraSensor;

}  // namespace app::sensor
