#pragma once

#include <Arduino.h>

#include "sensor_module.h"

namespace app::sensing {

class SensorManager {
 public:
  using SampleHandler = bool (*)(const SensorSample& sample, void* userData);

  static constexpr size_t MAX_MODULES = 8;

  bool beginAll();
  void pollAll();

  size_t collectSamples(SampleHandler handler, void* userData);
  size_t collectBootSamples(SampleHandler handler, void* userData);

  uint32_t featureBits();
  size_t moduleCount() const { return moduleCountValue; }

 private:
  friend class SensorRegistryBuilder;
  void ensureRegistryInitialized();
  bool registerModule(ISensorModule& module);

  bool initialized = false;
  bool started = false;
  ISensorModule* modules[MAX_MODULES] = {};
  size_t moduleCountValue = 0;
};

extern SensorManager sensorManager;

}  // namespace app::sensing
