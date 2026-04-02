#pragma once

#include "app/sensing/sensor_module.h"

namespace app::sensing {

class DhtSensorModule final : public ISensorModule {
 public:
  const char* id() const override;
  uint32_t featureBit() const override;

  bool begin() override;
  void poll() override;

  bool readSample(SensorSample& out) override;
  bool bootSample(SensorSample& out) override;

 private:
  bool started = false;
  uint32_t lastReadMs = 0;
};

}  // namespace app::sensing
