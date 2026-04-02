#pragma once

#include <Arduino.h>

namespace app::actuator {

class ServoDriver {
 public:
  bool attach(uint8_t channel, int pin);
  bool writeDeg10(uint8_t channel, uint16_t degree10);
  bool isAttached(uint8_t channel) const;

 private:
  static constexpr uint8_t kMaxChannels = 4;
  bool attached[kMaxChannels] = {false, false, false, false};
};

}  // namespace app::actuator
