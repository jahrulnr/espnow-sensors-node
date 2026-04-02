#include "servo_driver.h"

#include <driver/ledc.h>

namespace app::actuator {

namespace {

static constexpr uint32_t kServoFreqHz = 50;
static constexpr uint32_t kPulseMinUs = 500;
static constexpr uint32_t kPulseMaxUs = 2500;
static constexpr uint32_t kPeriodUs = 1000000UL / kServoFreqHz;
static constexpr uint8_t kDutyResolution = LEDC_TIMER_14_BIT;
static constexpr uint32_t kDutyMax = (1UL << 14) - 1;

ledc_channel_t toLedcChannel(uint8_t channel) {
  switch (channel) {
    case 0:
      return LEDC_CHANNEL_0;
    case 1:
      return LEDC_CHANNEL_1;
    case 2:
      return LEDC_CHANNEL_2;
    default:
      return LEDC_CHANNEL_3;
  }
}

uint32_t degree10ToDuty(uint16_t degree10) {
  if (degree10 > 1800) {
    degree10 = 1800;
  }

  const uint32_t pulseUs = kPulseMinUs + ((kPulseMaxUs - kPulseMinUs) * static_cast<uint32_t>(degree10) / 1800UL);
  return (pulseUs * kDutyMax) / kPeriodUs;
}

}  // namespace

bool ServoDriver::attach(uint8_t channel, int pin) {
  if (channel >= kMaxChannels || pin < 0) {
    return false;
  }

  const ledc_timer_config_t timerConfig = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = static_cast<ledc_timer_bit_t>(kDutyResolution),
      .timer_num = LEDC_TIMER_0,
      .freq_hz = static_cast<int>(kServoFreqHz),
      .clk_cfg = LEDC_AUTO_CLK,
      .deconfigure = false,
  };
  if (ledc_timer_config(&timerConfig) != ESP_OK) {
    return false;
  }

  const ledc_channel_config_t channelConfig = {
      .gpio_num = pin,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = toLedcChannel(channel),
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_0,
      .duty = 0,
      .hpoint = 0,
      .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
      .flags = {
          .output_invert = 0,
      },
  };

  if (ledc_channel_config(&channelConfig) != ESP_OK) {
    return false;
  }

  attached[channel] = true;
  return true;
}

bool ServoDriver::writeDeg10(uint8_t channel, uint16_t degree10) {
  if (channel >= kMaxChannels || !attached[channel]) {
    return false;
  }

  const uint32_t duty = degree10ToDuty(degree10);
  if (ledc_set_duty(LEDC_LOW_SPEED_MODE, toLedcChannel(channel), duty) != ESP_OK) {
    return false;
  }

  return ledc_update_duty(LEDC_LOW_SPEED_MODE, toLedcChannel(channel)) == ESP_OK;
}

bool ServoDriver::isAttached(uint8_t channel) const {
  if (channel >= kMaxChannels) {
    return false;
  }

  return attached[channel];
}

}  // namespace app::actuator
