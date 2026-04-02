#pragma once

#include <Arduino.h>

namespace app::actuation {

enum class ActuatorKind : uint8_t {
  Unknown = 0,
  Servo = 1,
};

enum class ActuationStatus : uint8_t {
  Ok = 0,
  Disabled = 1,
  InvalidGroup = 2,
  InvalidChannel = 3,
  InvalidValue = 4,
  DriverError = 5,
};

struct ServoActuationRequest {
  char group[16] = {0};
  uint8_t channel = 0;
  uint16_t targetDeg10 = 0;
  uint16_t transitionMs = 0;
};

struct ActuationRequest {
  ActuatorKind kind = ActuatorKind::Unknown;
  uint32_t timestampMs = 0;
  ServoActuationRequest servo = {};
};

struct ServoActuationState {
  char group[16] = {0};
  uint8_t channel = 0;
  uint16_t targetDeg10 = 0;
  uint16_t appliedDeg10 = 0;
};

struct ActuationResponse {
  bool ok = false;
  ActuationStatus status = ActuationStatus::DriverError;
  uint32_t timestampMs = 0;
  ServoActuationState servo = {};
};

}  // namespace app::actuation
