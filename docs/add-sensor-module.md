# Add Sensor Module

Panduan singkat menambah sensor baru tanpa mengubah logic network.

Lihat juga index wiki: [README.md](README.md)

## 1) Buat Module Adapter

Buat file di `src/app/sensing/modules/` yang implement `ISensorModule`:

- `id()`
- `featureBit()`
- `begin()`
- `poll()`
- `readSample()`
- `bootSample()`

Referensi implementasi:

- `dht_module.cpp`
- `mmwave_module.cpp`

## 2) Tambah Data Shape di Sensor Sample

Jika sensor butuh payload baru, tambahkan field data baru di:

- `src/app/sensing/sensor_sample.h`

Contoh: `struct MotionSampleData` lalu masukkan ke `SensorSample`.

## 3) Tambah Encoder Mapping

Mapping `SensorSample -> state_binary` ada di:

- `src/app/sensing/sensor_encoder.cpp`

Tambahkan `case SensorKind::<YourKind>` untuk menghasilkan payload binary.

## 4) Register Module ke SensorManager

Daftarkan module di:

- `src/app/sensing/sensor_manager.cpp`

Pola saat ini:

- deklarasi static instance module
- panggil `registerModule(...)` di `ensureRegistryInitialized()`

## 5) Tambah Feature Bit (opsional)

Jika perlu capability flag baru ke master:

- tambahkan bit di `state_binary.h`
- kembalikan bit itu dari `featureBit()` module

`FeaturesState` akan otomatis ikut karena `network/slave` membaca dari `sensorManager.featureBits()`.

## 6) Tambah Profile Config

Jika sensor hanya untuk board tertentu, update profile:

- `include/profiles/profile_*.h`

Lalu pakai macro profile itu di module adapter.

## 7) Validasi

Minimal jalankan:

```bash
platformio run -e esp32-c3-super-mini
```

Jika menambah profile baru, build env profile tersebut juga.
