# Add Sensor Module

A short guide to add a new sensor without changing network logic.

See wiki index: [README.md](README.md)

## 1) Create Module Adapter

Create files in `src/app/sensing/modules/` implementing `ISensorModule`:

- `id()`
- `featureBit()`
- `begin()`
- `poll()`
- `readSample()`
- `bootSample()`

Implementation references:

- `dht_module.cpp`
- `mmwave_module.cpp`

## 2) Add Data Shape in Sensor Sample

If sensor needs a new payload shape, add data fields in:

- `src/app/sensing/sensor_sample.h`

Example: add `struct MotionSampleData`, then include it in `SensorSample`.

## 3) Add Encoder Mapping

`SensorSample -> state_binary` mapping is in:

- `src/app/sensing/sensor_encoder.cpp`

Add `case SensorKind::<YourKind>` to produce binary payload.

## 4) Register Module in SensorManager

Register module in:

- `src/app/sensing/sensor_manager.cpp`

Current pattern:

- declare static module instance
- call `registerModule(...)` in `ensureRegistryInitialized()`

## 5) Add Feature Bit (Optional)

If you need a new capability flag for master:

- add the bit in `state_binary.h`
- return that bit from module `featureBit()`

`FeaturesState` will include it automatically because `network/slave` reads from `sensorManager.featureBits()`.

## 6) Add Profile Config

If sensor is board-specific, update profile:

- `include/profiles/profile_*.h`

Then use those profile macros in the module adapter.

## 7) Validate

At minimum run:

```bash
platformio run -e esp32-c3-super-mini
```

If you add a new profile, build that profile environment too.

## 8) Reusable Filtering (Optional)

If new sensor needs filtering/noise handling, reuse components in:
- `src/app/algorithms/`

This keeps filter logic reusable across modules.

Indonesian version: [add-sensor-module_id.md](add-sensor-module_id.md)
