#pragma once

#include <Arduino.h>

#include "sensor_sample.h"

namespace app::sensing {

const char* sensorKindName(SensorKind kind);

bool encodeSampleToStateBinary(const SensorSample& sample,
                               uint8_t* outPayload,
                               size_t outCapacity,
                               size_t& outSize);

}  // namespace app::sensing
