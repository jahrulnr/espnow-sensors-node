#include "camera_module.h"

#include "app/espnow/state_binary.h"
#include "app/sensor/camera_sensor.h"

#include <app_config.h>

namespace app::sensing {

const char* CameraSensorModule::id() const {
  return "camera";
}

uint32_t CameraSensorModule::featureBit() const {
#if CAMERA_SENSOR_ENABLED
  return static_cast<uint32_t>(app::espnow::state_binary::FeatureCameraJpeg)
       | static_cast<uint32_t>(app::espnow::state_binary::FeatureCameraStream);
#else
  return 0;
#endif
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
