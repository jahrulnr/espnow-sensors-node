# ESP-NOW Sensors Node

[![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-orange?logo=platformio)](https://platformio.org/)
![MCU](https://img.shields.io/badge/MCU-ESP32--C3%20%7C%20ESP32--S3-1f6feb)
![Protocol](https://img.shields.io/badge/protocol-v1-2ea44f)
![WiFi](https://img.shields.io/badge/WiFi-command--driven-blue)
[![Docs](https://img.shields.io/badge/docs-wiki-0ea5e9)](docs/README_id.md)

Node modular berbasis ESP32 untuk telemetri dan kontrol perangkat lintas board/profile.

## Untuk Siapa Proyek Ini

Proyek ini cocok untuk:

- **Smart home builder** — pasang node sensor + aktuator di berbagai sudut rumah, dikontrol dari satu master lewat ESP-NOW tanpa bergantung pada layanan cloud.
- **Sistem IoT berbasis AI agent** — slave mengekspos identitas dan daftar module-nya via protokol ESP-NOW, sehingga AI agent di sisi master dapat menemukan kapabilitas node secara dinamis dan langsung bertindak (contoh: "putar servo ke 90°", "ada gerakan di ruang 2?").
- **Maker dan penghobi embedded** — ganti board target atau kombinasi module sensor/aktuator cukup lewat profile config, tanpa perlu menulis ulang firmware.
- **Skenario multi-node** — jalankan beberapa slave dengan profile berbeda, semuanya melapor ke satu master menggunakan kontrak protokol binary yang sama.

## Apa Tujuan Proyek Ini

Tujuan utama proyek ini:
- Membuat node fleksibel di berbagai board target.
- Kombinasi module sensing dan kontrol bisa diganti lewat konfigurasi profile.
- Jalur `boot`, `input`, dan `network` tetap terpisah agar mudah dirawat dan dikembangkan.
- Menyediakan kontrak binary yang stabil agar integrasi sisi master dapat menemukan dan berinteraksi dengan node slave mana pun tanpa perlu hardcode kapabilitas.

Contoh deployment:
- ESP32-C3: `mmwave + dht` → node sensor kehadiran + suhu/kelembaban
- ESP32-S3: `dht` → node iklim ringan dengan deep sleep
- ESP32-CAM (AI Thinker): `camera + kontrol ESP-NOW-first` → node kamera dengan servo pan/tilt
- Board lainnya: tambah profile, tambah module, flash dan langsung jalan.

## Mulai Dari Sini

Kalau kamu ingin:
- Lihat peta dokumentasi: [docs/README_id.md](docs/README_id.md)
- Paham arsitektur modular: [docs/architecture_id.md](docs/architecture_id.md)
- Atur profile board, pin, dan powersave: [docs/configuration_id.md](docs/configuration_id.md)
- Build dan verifikasi: [docs/build-and-verify_id.md](docs/build-and-verify_id.md)
- Integrasi master berdasarkan kontrak protocol: [docs/api-contract_id.md](docs/api-contract_id.md)
- Integrasi client lewat WebSocket gateway: [docs/websocket-contract_id.md](docs/websocket-contract_id.md)
- Tambah WebSocket hook custom: [docs/add-websocket-hook_id.md](docs/add-websocket-hook_id.md)
- Tambah jenis sensor baru: [docs/add-sensor-module_id.md](docs/add-sensor-module_id.md)
- Discovery module slave via ESP-NOW: [docs/api-contract_id.md](docs/api-contract_id.md)

## Catatan

- Detail teknis dipusatkan di folder [docs](docs/).
- Versi English: [README.md](README.md)
