#include "sensor_encoder.h"

#include "app/espnow/state_binary.h"

#include <cstring>

namespace app::sensing {

const char* sensorKindName(SensorKind kind) {
  switch (kind) {
    case SensorKind::Dht:
      return "dht";
    case SensorKind::Mmwave:
      return "mmwave";
    case SensorKind::Battery:
      return "battery";
    case SensorKind::Wakeup:
      return "wakeup";
    default:
      return "unknown";
  }
}

bool encodeSampleToStateBinary(const SensorSample& sample,
                               uint8_t* outPayload,
                               size_t outCapacity,
                               size_t& outSize) {
  outSize = 0;
  if (!sample.valid || outPayload == nullptr || outCapacity == 0) {
    return false;
  }

  switch (sample.kind) {
    case SensorKind::Dht: {
      app::espnow::state_binary::SensorState state = {};
      app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Sensor);
      state.temperature10 = static_cast<int16_t>(sample.dht.temperatureC * 10.0f);
      state.humidity10 = static_cast<uint16_t>(sample.dht.humidityPercent * 10.0f);

      const size_t bytes = sizeof(state);
      if (outCapacity < bytes) {
        return false;
      }

      memcpy(outPayload, &state, bytes);
      outSize = bytes;
      return true;
    }

    case SensorKind::Mmwave: {
      app::espnow::state_binary::MmwaveState state = {};
      app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Mmwave);
      state.detected = sample.mmwave.detected ? 1 : 0;
      state.hasDistance = sample.mmwave.hasDistance ? 1 : 0;
      state.distanceCm = sample.mmwave.distanceCm;
      state.frameCount = sample.mmwave.frameCount;
      state.byteCount = sample.mmwave.byteCount;

      const size_t bytes = sizeof(state);
      if (outCapacity < bytes) {
        return false;
      }

      memcpy(outPayload, &state, bytes);
      outSize = bytes;
      return true;
    }

    default:
      return false;
  }
}

}  // namespace app::sensing
