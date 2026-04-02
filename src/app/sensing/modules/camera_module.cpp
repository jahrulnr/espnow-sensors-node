#include "camera_module.h"

#include "app/sensor/camera_sensor.h"

namespace app::sensing {

const char* CameraSensorModule::id() const {
  return "camera";
}

uint32_t CameraSensorModule::featureBit() const {
  return 0;
}

bool CameraSensorModule::begin() {
  return app::sensor::cameraSensor.begin();
}

void CameraSensorModule::poll() {
}

bool CameraSensorModule::readSample(SensorSample& out) {
  (void)out;
  return false;
}

bool CameraSensorModule::bootSample(SensorSample& out) {
  (void)out;
  return false;
}

}  // namespace app::sensing
