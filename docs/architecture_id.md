# Architecture

## Prinsip

- Sensor modular via interface `ISensorModule`.
- Actuator modular via interface `IActuatorModule`.
- `networkTask` tidak membaca driver sensor langsung.
- `boot`, `input`, dan `network` punya tanggung jawab terpisah.

## Komponen Utama

- Kontrak module sensor:
  - `src/app/sensing/sensor_module.h`
- Bentuk data sample lintas sensor:
  - `src/app/sensing/sensor_sample.h`
- Registry sensor aktif:
  - `src/app/sensing/sensor_manager.h`
  - `src/app/sensing/sensor_manager.cpp`
- Encoder sample ke payload binary:
  - `src/app/sensing/sensor_encoder.h`
  - `src/app/sensing/sensor_encoder.cpp`
- Kontrak module actuator dan manager:
  - `src/app/actuation/actuator_module.h`
  - `src/app/actuation/actuator_manager.h`
  - `src/app/actuation/actuator_manager.cpp`
- Driver output device contoh:
  - `src/app/actuator/servo_driver.h`
  - `src/app/actuator/servo_driver.cpp`
- WiFi control command-driven (opsional, layer network):
  - `src/app/network/wifi_manager.h`
  - `src/app/network/wifi_manager.cpp`
- Reusable signal-processing algorithms:
  - `src/app/algorithms/` (binary state filter, median window, EMA, trimmed mean)

## Flow Runtime

1. `setup()` memanggil `app::boot::run()`.
2. `boot` inisialisasi sensor via `SensorManager`.
3. `boot` inisialisasi actuator via `ActuatorManager`.
4. `boot` ambil 1 sample awal per module sensor, lalu log.
5. `networkTask` handle link ESP-NOW + publish identity/features.
6. `inputTask` (saat non-powersave) polling sensor via manager dan publish payload encoded.

## Boundary Yang Dijaga

- Sensor logic tetap di layer sensing/module.
- Actuation logic tetap di layer actuation/module.
- Format payload diisolasi di encoder.
- Network hanya transport dan state link/master.
- Penambahan sensor baru tidak mengharuskan ubah `networkTask`.
- Discovery module untuk interoperabilitas master diekspos via kontrak command/state ESP-NOW.

English version: [architecture.md](architecture.md)
