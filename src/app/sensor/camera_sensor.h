#pragma once

#include <Arduino.h>

namespace app::sensor {

class CameraSensor {
 public:
  struct RuntimeConfig {
    bool ready = false;
    uint8_t frameSize = 0;
    uint8_t quality = 0;
    int8_t brightness = 0;
    int8_t contrast = 0;
    int8_t saturation = 0;
    bool hmirror = false;
    bool vflip = false;
    int xclkHz = 0;
  };

  CameraSensor() = default;

  bool begin();
  bool isReady() const { return ready; }
  bool getRuntimeConfig(RuntimeConfig& out) const;
  bool setFrameSize(uint8_t frameSize);
  bool setXclkHz(int xclkHz);
  bool setHmirror(bool enabled);
  bool setVflip(bool enabled);
  bool setQuality(uint8_t quality);
  bool setBrightness(int8_t level);
  bool setContrast(int8_t level);
  bool setSaturation(int8_t level);

 private:
  bool initAttempted = false;
  bool ready = false;
};

extern CameraSensor cameraSensor;

}  // namespace app::sensor
