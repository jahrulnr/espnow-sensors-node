# Configuration

## Configuration Structure

Configuration is split into two layers:
- Global policy: `include/app_config.h`
- Board profile (pins + enabled sensor set): `include/profiles/profile_*.h`

Profile selection is controlled by:
- `include/board_profile.h`
- `build_flags` in `platformio.ini`

User-local override (optional):
- If `include/profiles/profile_user.h` exists, it is selected first by `board_profile.h`.
- This lets each user keep custom pin mapping without changing tracked profile files.
- To customize quickly, copy an existing profile (for example `include/profiles/profile_esp32_c3.h`) to `include/profiles/profile_user.h`, then edit it locally.

## Available Profiles

### `BOARD_PROFILE_ESP32_C3`

- DHT: enabled
- mmWave: enabled
- Servo output: disabled by default
- mmWave pins:
  - TX: GPIO3
  - RX: GPIO4

### `BOARD_PROFILE_ESP32_S3`

- DHT: enabled
- mmWave: disabled (profile template)
- Servo output: disabled by default

### `BOARD_PROFILE_ESP32_CAM`

- Board target: AI Thinker ESP32-CAM (`esp32cam`)
- DHT: disabled (default)
- mmWave: disabled (default)
- Camera: enabled (pin map follows `esp32cam/pins.hpp` `AiThinker` reference)
- Servo output: disabled by default (reserved for named group `camera_pan_tilt`)
- Camera pins:
  - D0: GPIO5
  - D1: GPIO18
  - D2: GPIO19
  - D3: GPIO21
  - D4: GPIO36
  - D5: GPIO39
  - D6: GPIO34
  - D7: GPIO35
  - XCLK: GPIO0
  - PCLK: GPIO22
  - VSYNC: GPIO25
  - HREF: GPIO23
  - SDA: GPIO26
  - SCL: GPIO27
  - RESET: -1
  - PWDN: GPIO32

## Powersave

Available modes:
- Deep sleep (`POWERSAVE_SLEEP_MODE_DEEP`), default interval `10s`
- Light sleep (`POWERSAVE_SLEEP_MODE_LIGHT`), default interval `300ms`

Important macros in `include/app_config.h`:
- `ENABLE_POWERSAVE`
- `POWERSAVE_SLEEP_MODE`
- `POWERSAVE_DEEP_SLEEP_SEC`
- `POWERSAVE_LIGHT_SLEEP_MS`

## Sensor Filtering

mmWave filtering (enabled by default):
- `MMWAVE_FILTER_ENABLED`
- `MMWAVE_PRESENCE_ON_CONSECUTIVE`
- `MMWAVE_PRESENCE_OFF_CONSECUTIVE`
- `MMWAVE_PRESENCE_OFF_HOLD_MS`
- `MMWAVE_DISTANCE_MEDIAN_WINDOW`

Battery analog filtering:
- `BATTERY_FILTER_ENABLED`
- `BATTERY_ADC_SAMPLES`
- `BATTERY_ADC_TRIM_PERCENT`
- `BATTERY_VOLTAGE_EMA_ALPHA`

## Servo Output (Optional)

Servo output control is command-driven from master and supports named logical group routing.

Key macros in `include/app_config.h`:
- `SERVO_OUTPUT_ENABLED`
- `SERVO_GROUP_NAME`
- `SERVO_GROUP_CHANNEL_COUNT`
- `SERVO_MIN_DEG10`
- `SERVO_MAX_DEG10`
- `SERVO_DEFAULT_DEG10`

Board profile pin mapping:
- `BOARD_PROFILE_SERVO_PIN_CH0`
- `BOARD_PROFILE_SERVO_PIN_CH1`

## WiFi Mode (Optional)

WiFi STA support for advanced use cases (logging/dashboard/camera) is controlled by:
- `ENABLE_WIFI_MODE` (default `0`)
- `WIFI_CONNECT_TIMEOUT_MS` (default `15000`)
- `WIFI_CLIENT_HOSTNAME` (default `DEVICE_NAME`)

WebSocket gateway settings:
- `ENABLE_WEBSOCKET_GATEWAY` (default `1`, active only when WiFi mode is enabled and connected)
- `WEBSOCKET_SERVER_PORT` (default `81`)
- `WEBSOCKET_MAX_HOOKS` (default `8`)
- `WEBSOCKET_CAMERA_DEFAULT_WIDTH` (default `320`)
- `WEBSOCKET_CAMERA_DEFAULT_HEIGHT` (default `240`)
- `WEBSOCKET_CAMERA_DEFAULT_FORMAT` (default `"jpg"`)
- `WEBSOCKET_CAMERA_XCLK_HZ` (default `20000000`)
- `WEBSOCKET_CAMERA_JPEG_QUALITY` (default `12`)
- `WEBSOCKET_CAMERA_FB_COUNT` (default `2`)
- `WEBSOCKET_STREAM_MAX_CLIENTS` (default `4`)
- `WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS` (default `500`)
- `WEBSOCKET_STREAM_MIN_INTERVAL_MS` (default `120`)

Behavior:
- Node does not auto-connect WiFi on boot.
- Node only attempts WiFi connection if:
  1. `ENABLE_WIFI_MODE == 1`
  2. master sends WiFi credentials command.
- If master does not send credentials, node skips WiFi connection.
- When connected, node publishes WiFi hostname and mDNS as `<hostname>.local`.
- WebSocket server starts only after WiFi is connected and provides type-based hooks for multipurpose requests.

Ready build environment:
- `esp32-c3-super-mini-wifi` (sets `ENABLE_WIFI_MODE=1`)
- `esp32-cam-ai-thinker` (sets `BOARD_PROFILE_ESP32_CAM` and `ENABLE_WIFI_MODE=1`)

## Boot Behavior

Before attempting to link with master:
1. Boot initializes all active sensors.
2. Takes one initial sample from each module.
3. Writes those samples to logs.

If master is not connected, powersave flow still continues to the sleep cycle.

Indonesian version: [configuration_id.md](configuration_id.md)
