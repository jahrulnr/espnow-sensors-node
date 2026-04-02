# Architecture

## Principles

- Sensors are modular through the `ISensorModule` interface.
- `networkTask` does not read sensor drivers directly.
- `boot`, `input`, and `network` have separate responsibilities.

## Main Components

- Sensor module contract:
  - `src/app/sensing/sensor_module.h`
- Cross-sensor sample data shape:
  - `src/app/sensing/sensor_sample.h`
- Active sensor registry:
  - `src/app/sensing/sensor_manager.h`
  - `src/app/sensing/sensor_manager.cpp`
- Sample-to-binary payload encoder:
  - `src/app/sensing/sensor_encoder.h`
  - `src/app/sensing/sensor_encoder.cpp`
- Command-driven WiFi control (optional, network layer):
  - `src/app/network/wifi_manager.h`
  - `src/app/network/wifi_manager.cpp`
- Reusable signal-processing algorithms:
  - `src/app/algorithms/` (binary state filter, median window, EMA, trimmed mean)

## Runtime Flow

1. `setup()` calls `app::boot::run()`.
2. `boot` initializes sensors through `SensorManager`.
3. `boot` takes one initial sample per active module and logs it.
4. `networkTask` handles ESP-NOW link management and publishes identity/features.
5. `inputTask` (when powersave is disabled) polls sensors through manager and publishes encoded payloads.

## Protected Boundaries

- Sensor logic stays in sensing/module layer.
- Payload format is isolated in encoder.
- Network layer handles transport and link/master state only.
- Adding a new sensor should not require changing `networkTask`.

Indonesian version: [architecture_id.md](architecture_id.md)
