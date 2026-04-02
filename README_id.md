# ESP-NOW Sensors Node

[![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-orange?logo=platformio)](https://platformio.org/)
![MCU](https://img.shields.io/badge/MCU-ESP32--C3%20%7C%20ESP32--S3-1f6feb)
![Protocol](https://img.shields.io/badge/protocol-v1-2ea44f)
![WiFi](https://img.shields.io/badge/WiFi-command--driven-blue)
[![Docs](https://img.shields.io/badge/docs-wiki-0ea5e9)](docs/README_id.md)

Node sensor modular berbasis ESP32 untuk skenario multi-purpose sensors collector lintas board/profile.

## Proyek Ini Untuk Apa

Tujuan utama proyek ini:
- Menjadikan node sensor fleksibel per board.
- Kombinasi sensor bisa diganti lewat konfigurasi profile.
- Jalur `boot`, `input`, dan `network` tetap terpisah supaya tidak gemuk.

Contoh skenario:
- ESP32-C3: `mmwave + dht`
- ESP32-S3: `dht`
- Board lain: tinggal tambah profile dan module sensor.

## Mulai Dari Sini

Kalau kamu ingin:
- Lihat peta dokumentasi: [docs/README_id.md](docs/README_id.md)
- Paham arsitektur modular: [docs/architecture_id.md](docs/architecture_id.md)
- Atur profile board, pin, dan powersave: [docs/configuration_id.md](docs/configuration_id.md)
- Build dan verifikasi: [docs/build-and-verify_id.md](docs/build-and-verify_id.md)
- Integrasi master berdasarkan kontrak protocol: [docs/api-contract_id.md](docs/api-contract_id.md)
- Tambah jenis sensor baru: [docs/add-sensor-module_id.md](docs/add-sensor-module_id.md)

## Catatan

- Detail teknis dipusatkan di folder [docs](docs/).
- Versi English: [README.md](README.md)
