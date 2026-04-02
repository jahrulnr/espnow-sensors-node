# Architecture

## Principles

- Sensors are modular through the `ISensorModule` interface.
- Actuators are modular through the `IActuatorModule` interface.
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
- Actuator module contract and manager:
  - `src/app/actuation/actuator_module.h`
  - `src/app/actuation/actuator_manager.h`
  - `src/app/actuation/actuator_manager.cpp`
- Output device driver example:
  - `src/app/actuator/servo_driver.h`
  - `src/app/actuator/servo_driver.cpp`
- Command-driven WiFi control (optional, network layer):
  - `src/app/network/wifi_manager.h`
  - `src/app/network/wifi_manager.cpp`
- Reusable signal-processing algorithms:
  - `src/app/algorithms/` (binary state filter, median window, EMA, trimmed mean)

## Runtime Flow

1. `setup()` calls `app::boot::run()`.
2. `boot` initializes sensors through `SensorManager`.
3. `boot` initializes actuator modules through `ActuatorManager`.
4. `boot` takes one initial sample per active sensor module and logs it.
5. `networkTask` handles ESP-NOW link management and publishes identity/features.
6. `inputTask` (when powersave is disabled) polls sensors through manager and publishes encoded payloads.

## Protected Boundaries

- Sensor logic stays in sensing/module layer.
- Actuation logic stays in actuation/module layer.
- Payload format is isolated in encoder.
- Network layer handles transport and link/master state only.
- Adding a new sensor should not require changing `networkTask`.

Indonesian version: [architecture_id.md](architecture_id.md)
