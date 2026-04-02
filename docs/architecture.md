# Architecture

## Prinsip

- Sensor modular via interface `ISensorModule`.
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
- WiFi control command-driven (opsional, layer network):
  - `src/app/network/wifi_manager.h`
  - `src/app/network/wifi_manager.cpp`

## Flow Runtime

1. `setup()` memanggil `app::boot::run()`.
2. `boot` inisialisasi sensor via `SensorManager`.
3. `boot` ambil 1 sample awal per module, lalu log.
4. `networkTask` handle link ESP-NOW + publish identity/features.
5. `inputTask` (saat non-powersave) polling sensor via manager dan publish payload encoded.

## Boundary Yang Dijaga

- Sensor logic tetap di layer sensing/module.
- Format payload diisolasi di encoder.
- Network hanya transport dan state link/master.
- Penambahan sensor baru tidak mengharuskan ubah `networkTask`.
