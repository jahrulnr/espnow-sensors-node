#include "camera_sensor.h"

#include <app_config.h>
#include <hw.h>

#if CAMERA_SENSOR_ENABLED
#include <esp_camera.h>
#include <esp_log.h>
#endif

namespace app::sensor {

namespace {

#if CAMERA_SENSOR_ENABLED
static constexpr const char* TAG = "CAMERA_SENSOR";
#endif

}  // namespace

CameraSensor cameraSensor;

bool CameraSensor::begin() {
#if !CAMERA_SENSOR_ENABLED
  return true;
#else
  if (initAttempted) {
    return ready;
  }

  initAttempted = true;

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAMERA_PIN_D0;
  config.pin_d1 = CAMERA_PIN_D1;
  config.pin_d2 = CAMERA_PIN_D2;
  config.pin_d3 = CAMERA_PIN_D3;
  config.pin_d4 = CAMERA_PIN_D4;
  config.pin_d5 = CAMERA_PIN_D5;
  config.pin_d6 = CAMERA_PIN_D6;
  config.pin_d7 = CAMERA_PIN_D7;
  config.pin_xclk = CAMERA_PIN_XCLK;
  config.pin_pclk = CAMERA_PIN_PCLK;
  config.pin_vsync = CAMERA_PIN_VSYNC;
  config.pin_href = CAMERA_PIN_HREF;
  config.pin_sccb_sda = CAMERA_PIN_SDA;
  config.pin_sccb_scl = CAMERA_PIN_SCL;
  config.pin_pwdn = CAMERA_PIN_PWDN;
  config.pin_reset = CAMERA_PIN_RESET;
  config.xclk_freq_hz = WEBSOCKET_CAMERA_XCLK_HZ;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = WEBSOCKET_CAMERA_JPEG_QUALITY;
  config.fb_count = WEBSOCKET_CAMERA_FB_COUNT;
#if defined(CAMERA_GRAB_LATEST)
  config.grab_mode = CAMERA_GRAB_LATEST;
#endif
#if defined(CAMERA_FB_IN_PSRAM) && defined(CAMERA_FB_IN_DRAM)
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
#endif

  if (config.pin_d0 < 0 || config.pin_d1 < 0 || config.pin_d2 < 0 || config.pin_d3 < 0 || config.pin_d4 < 0 ||
      config.pin_d5 < 0 || config.pin_d6 < 0 || config.pin_d7 < 0 || config.pin_xclk < 0 || config.pin_pclk < 0 ||
      config.pin_vsync < 0 || config.pin_href < 0 || config.pin_sccb_sda < 0 || config.pin_sccb_scl < 0) {
    ESP_LOGW(TAG, "Camera pin profile invalid, skip init");
    ready = false;
    return false;
  }

  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    ready = false;
    return false;
  }

  ready = true;
  ESP_LOGI(TAG, "Camera initialized");
  return true;
#endif
}

}  // namespace app::sensor
