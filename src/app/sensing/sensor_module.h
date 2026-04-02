#pragma once

#include <Arduino.h>

#include "sensor_sample.h"

namespace app::sensing {

class ISensorModule {
 public:
  virtual ~ISensorModule() = default;

  virtual const char* id() const = 0;
  virtual uint32_t featureBit() const = 0;

  virtual bool begin() = 0;
  virtual void poll() = 0;

  virtual bool readSample(SensorSample& out) = 0;
  virtual bool bootSample(SensorSample& out) = 0;
};

}  // namespace app::sensing
