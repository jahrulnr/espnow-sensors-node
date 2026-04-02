# Configuration

## Struktur Konfigurasi

Konfigurasi dibagi dua lapis:
- Global policy: `include/app_config.h`
- Board profile (pin + sensor set): `include/profiles/profile_*.h`

Pemilihan profile lewat:
- `include/board_profile.h`
- `build_flags` di `platformio.ini`

## Profile Yang Tersedia

### `BOARD_PROFILE_ESP32_C3`

- DHT: aktif
- mmWave: aktif
- Pin mmWave:
  - TX: GPIO3
  - RX: GPIO4

### `BOARD_PROFILE_ESP32_S3`

- DHT: aktif
- mmWave: nonaktif (template profile)

## Powersave

Mode yang tersedia:
- Deep sleep (`POWERSAVE_SLEEP_MODE_DEEP`), default interval `10s`
- Light sleep (`POWERSAVE_SLEEP_MODE_LIGHT`), default interval `300ms`

Macro penting di `include/app_config.h`:
- `ENABLE_POWERSAVE`
- `POWERSAVE_SLEEP_MODE`
- `POWERSAVE_DEEP_SLEEP_SEC`
- `POWERSAVE_LIGHT_SLEEP_MS`

## WiFi Mode (Opsional)

Fitur WiFi STA untuk use-case lanjut (logging/dashboard/camera) dikontrol oleh macro:
- `ENABLE_WIFI_MODE` (default `0`)
- `WIFI_CONNECT_TIMEOUT_MS` (default `15000`)
- `WIFI_CLIENT_HOSTNAME` (default `DEVICE_NAME`)

Perilaku:
- Node tidak akan connect WiFi otomatis saat boot.
- Node hanya akan connect WiFi jika:
  1. `ENABLE_WIFI_MODE == 1`
  2. master mengirim command kredensial WiFi.
- Jika master tidak mengirim kredensial, node skip koneksi WiFi.
- Saat koneksi berhasil, node publish hostname WiFi dan mDNS di `<hostname>.local`.

Env build siap pakai:
- `esp32-c3-super-mini-wifi` (set `ENABLE_WIFI_MODE=1`)

## Behavior Boot

Sebelum coba link ke master:
1. Boot init semua sensor aktif.
2. Ambil 1 sample awal dari setiap module.
3. Tulis sample ke log.

Jika master tidak terhubung, flow powersave tetap lanjut ke cycle sleep.
