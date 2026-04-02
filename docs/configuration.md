# Configuration

## Configuration Structure

Configuration is split into two layers:
- Global policy: `include/app_config.h`
- Board profile (pins + enabled sensor set): `include/profiles/profile_*.h`

Profile selection is controlled by:
- `include/board_profile.h`
- `build_flags` in `platformio.ini`

## Available Profiles

### `BOARD_PROFILE_ESP32_C3`

- DHT: enabled
- mmWave: enabled
- mmWave pins:
  - TX: GPIO3
  - RX: GPIO4

### `BOARD_PROFILE_ESP32_S3`

- DHT: enabled
- mmWave: disabled (profile template)

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

## WiFi Mode (Optional)

WiFi STA support for advanced use cases (logging/dashboard/camera) is controlled by:
- `ENABLE_WIFI_MODE` (default `0`)
- `WIFI_CONNECT_TIMEOUT_MS` (default `15000`)
- `WIFI_CLIENT_HOSTNAME` (default `DEVICE_NAME`)

Behavior:
- Node does not auto-connect WiFi on boot.
- Node only attempts WiFi connection if:
  1. `ENABLE_WIFI_MODE == 1`
  2. master sends WiFi credentials command.
- If master does not send credentials, node skips WiFi connection.
- When connected, node publishes WiFi hostname and mDNS as `<hostname>.local`.

Ready build environment:
- `esp32-c3-super-mini-wifi` (sets `ENABLE_WIFI_MODE=1`)

## Boot Behavior

Before attempting to link with master:
1. Boot initializes all active sensors.
2. Takes one initial sample from each module.
3. Writes those samples to logs.

If master is not connected, powersave flow still continues to the sleep cycle.

Indonesian version: [configuration_id.md](configuration_id.md)
