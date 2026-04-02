#include "sensor_manager.h"

#include "modules/camera_module.h"
#include "modules/dht_module.h"
#include "modules/mmwave_module.h"

namespace app::sensing {

namespace {

DhtSensorModule dhtModule;
MmwaveSensorModule mmwaveModule;
CameraSensorModule cameraModule;

}  // namespace

SensorManager sensorManager;

bool SensorManager::registerModule(ISensorModule& module) {
  if (moduleCountValue >= MAX_MODULES) {
    return false;
  }

  modules[moduleCountValue++] = &module;
  return true;
}

void SensorManager::ensureRegistryInitialized() {
  if (!initialized) {
    registerModule(dhtModule);
    registerModule(mmwaveModule);
    registerModule(cameraModule);
    initialized = true;
  }
}

bool SensorManager::beginAll() {
  ensureRegistryInitialized();

  if (started) {
    return true;
  }

  bool ok = true;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    ok = modules[i]->begin() && ok;
  }

  started = true;
  return ok;
}

void SensorManager::pollAll() {
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }
    modules[i]->poll();
  }
}

size_t SensorManager::collectSamples(SampleHandler handler, void* userData) {
  ensureRegistryInitialized();

  if (handler == nullptr) {
    return 0;
  }

  size_t emitted = 0;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    SensorSample sample;
    if (!modules[i]->readSample(sample)) {
      continue;
    }

    if (handler(sample, userData)) {
      emitted++;
    }
  }

  return emitted;
}

size_t SensorManager::collectBootSamples(SampleHandler handler, void* userData) {
  ensureRegistryInitialized();

  if (handler == nullptr) {
    return 0;
  }

  size_t emitted = 0;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }

    SensorSample sample;
    if (!modules[i]->bootSample(sample)) {
      continue;
    }

    if (handler(sample, userData)) {
      emitted++;
    }
  }

  return emitted;
}

uint32_t SensorManager::featureBits() {
  ensureRegistryInitialized();

  uint32_t bits = 0;
  for (size_t i = 0; i < moduleCountValue; ++i) {
    if (modules[i] == nullptr) {
      continue;
    }
    bits |= modules[i]->featureBit();
  }
  return bits;
}

}  // namespace app::sensing
