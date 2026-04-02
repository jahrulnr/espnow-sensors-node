#pragma once

#include "app/algorithms/binary_state_filter.h"
#include "app/algorithms/median_window_u16.h"
#include "app/sensing/sensor_module.h"

namespace app::sensing {

class MmwaveSensorModule final : public ISensorModule {
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
  app::algorithms::BinaryStateFilter presenceFilter;
  app::algorithms::MedianWindowU16 distanceFilter;
};

}  // namespace app::sensing
