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

sensor_t* getSensorHandle() {
  return esp_camera_sensor_get();
}
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
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

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

bool CameraSensor::getRuntimeConfig(RuntimeConfig& out) const {
#if !CAMERA_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!ready) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr) {
    return false;
  }

  out.ready = true;
  out.frameSize = static_cast<uint8_t>(sensor->status.framesize);
  out.quality = sensor->status.quality;
  out.brightness = sensor->status.brightness;
  out.contrast = sensor->status.contrast;
  out.saturation = sensor->status.saturation;
  out.hmirror = sensor->status.hmirror != 0;
  out.vflip = sensor->status.vflip != 0;
  out.xclkHz = sensor->xclk_freq_hz;
  return true;
#endif
}

bool CameraSensor::captureMeta(CaptureMeta& out) {
#if !CAMERA_SENSOR_ENABLED
  (void)out;
  return false;
#else
  if (!begin()) {
    return false;
  }

  for (uint32_t attempt = 0; attempt < CAMERA_META_CAPTURE_RETRIES; ++attempt) {
    const uint32_t captureStartMs = millis();
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
      delay(CAMERA_META_RETRY_DELAY_MS);
      continue;
    }

    if (fb->buf == nullptr || fb->len == 0) {
      esp_camera_fb_return(fb);
      delay(CAMERA_META_RETRY_DELAY_MS);
      continue;
    }

    out.width = static_cast<uint16_t>(fb->width);
    out.height = static_cast<uint16_t>(fb->height);
    out.frameBytes = static_cast<uint32_t>(fb->len);
    out.latencyMs = static_cast<uint16_t>(millis() - captureStartMs);
    out.frameFormat = fb->format == PIXFORMAT_JPEG ? 1 : 0;

    sensor_t* sensor = getSensorHandle();
    out.cameraType = sensor != nullptr ? static_cast<uint8_t>(sensor->id.PID & 0xFF) : 0;

    esp_camera_fb_return(fb);
    return true;
  }

  return false;
#endif
}

bool CameraSensor::setFrameSize(uint8_t frameSize) {
#if !CAMERA_SENSOR_ENABLED
  (void)frameSize;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_framesize == nullptr) {
    return false;
  }

  return sensor->set_framesize(sensor, static_cast<framesize_t>(frameSize)) == 0;
#endif
}

bool CameraSensor::setXclkHz(int xclkHz) {
#if !CAMERA_SENSOR_ENABLED
  (void)xclkHz;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_xclk == nullptr) {
    return false;
  }

  return sensor->set_xclk(sensor, LEDC_TIMER_0, xclkHz) == 0;
#endif
}

bool CameraSensor::setHmirror(bool enabled) {
#if !CAMERA_SENSOR_ENABLED
  (void)enabled;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_hmirror == nullptr) {
    return false;
  }

  return sensor->set_hmirror(sensor, enabled ? 1 : 0) == 0;
#endif
}

bool CameraSensor::setVflip(bool enabled) {
#if !CAMERA_SENSOR_ENABLED
  (void)enabled;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_vflip == nullptr) {
    return false;
  }

  return sensor->set_vflip(sensor, enabled ? 1 : 0) == 0;
#endif
}

bool CameraSensor::setQuality(uint8_t quality) {
#if !CAMERA_SENSOR_ENABLED
  (void)quality;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_quality == nullptr) {
    return false;
  }

  return sensor->set_quality(sensor, static_cast<int>(quality)) == 0;
#endif
}

bool CameraSensor::setBrightness(int8_t level) {
#if !CAMERA_SENSOR_ENABLED
  (void)level;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_brightness == nullptr) {
    return false;
  }

  return sensor->set_brightness(sensor, static_cast<int>(level)) == 0;
#endif
}

bool CameraSensor::setContrast(int8_t level) {
#if !CAMERA_SENSOR_ENABLED
  (void)level;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_contrast == nullptr) {
    return false;
  }

  return sensor->set_contrast(sensor, static_cast<int>(level)) == 0;
#endif
}

bool CameraSensor::setSaturation(int8_t level) {
#if !CAMERA_SENSOR_ENABLED
  (void)level;
  return false;
#else
  if (!begin()) {
    return false;
  }

  sensor_t* sensor = getSensorHandle();
  if (sensor == nullptr || sensor->set_saturation == nullptr) {
    return false;
  }

  return sensor->set_saturation(sensor, static_cast<int>(level)) == 0;
#endif
}

}  // namespace app::sensor
