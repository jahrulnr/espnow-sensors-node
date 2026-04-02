#include "servo_module.h"

#include "app/espnow/state_binary.h"

#include <app_config.h>
#include <hw.h>

#include <cstring>

namespace app::actuation {

namespace {

static constexpr size_t kGroupNameBytes = 16;

}  // namespace

const char* ServoModule::id() const {
  return "servo";
}

uint32_t ServoModule::featureBit() const {
#if SERVO_OUTPUT_ENABLED
  return static_cast<uint32_t>(app::espnow::state_binary::FeatureControlBasic)
       | static_cast<uint32_t>(app::espnow::state_binary::FeatureActuationServo);
#else
  return 0;
#endif
}

bool ServoModule::begin() {
#if !SERVO_OUTPUT_ENABLED
  started = false;
  return true;
#else
  if (started) {
    return true;
  }

  bool ok = true;
  if (SERVO_GROUP_CHANNEL_COUNT > 0) {
    if (SERVO_PIN_CH0 >= 0) {
      ok = driver.attach(0, SERVO_PIN_CH0) && ok;
      ok = driver.writeDeg10(0, SERVO_DEFAULT_DEG10) && ok;
      appliedDeg10[0] = SERVO_DEFAULT_DEG10;
    } else {
      ok = false;
    }
  }

  if (SERVO_GROUP_CHANNEL_COUNT > 1) {
    if (SERVO_PIN_CH1 >= 0) {
      ok = driver.attach(1, SERVO_PIN_CH1) && ok;
      ok = driver.writeDeg10(1, SERVO_DEFAULT_DEG10) && ok;
      appliedDeg10[1] = SERVO_DEFAULT_DEG10;
    } else {
      ok = false;
    }
  }

  started = ok;
  return started;
#endif
}

void ServoModule::poll() {
}

bool ServoModule::groupMatches(const char* groupName) const {
  if (groupName == nullptr || groupName[0] == '\0') {
    return false;
  }

  return strncmp(groupName, SERVO_GROUP_NAME, kGroupNameBytes) == 0;
}

bool ServoModule::handle(const ActuationRequest& request, ActuationResponse& response) {
  if (request.kind != ActuatorKind::Servo) {
    return false;
  }

  response = {};
  response.timestampMs = millis();
  strncpy(response.servo.group, request.servo.group, sizeof(response.servo.group) - 1);
  response.servo.group[sizeof(response.servo.group) - 1] = '\0';
  response.servo.channel = request.servo.channel;
  response.servo.targetDeg10 = request.servo.targetDeg10;

#if !SERVO_OUTPUT_ENABLED
  response.ok = false;
  response.status = ActuationStatus::Disabled;
  return true;
#else
  if (!started && !begin()) {
    response.ok = false;
    response.status = ActuationStatus::DriverError;
    return true;
  }

  if (!groupMatches(request.servo.group)) {
    response.ok = false;
    response.status = ActuationStatus::InvalidGroup;
    return true;
  }

  if (request.servo.channel >= SERVO_GROUP_CHANNEL_COUNT) {
    response.ok = false;
    response.status = ActuationStatus::InvalidChannel;
    return true;
  }

  if (request.servo.targetDeg10 < SERVO_MIN_DEG10 || request.servo.targetDeg10 > SERVO_MAX_DEG10) {
    response.ok = false;
    response.status = ActuationStatus::InvalidValue;
    return true;
  }

  if (!driver.writeDeg10(request.servo.channel, request.servo.targetDeg10)) {
    response.ok = false;
    response.status = ActuationStatus::DriverError;
    return true;
  }

  appliedDeg10[request.servo.channel] = request.servo.targetDeg10;
  response.ok = true;
  response.status = ActuationStatus::Ok;
  response.servo.appliedDeg10 = appliedDeg10[request.servo.channel];
  return true;
#endif
}

}  // namespace app::actuation
