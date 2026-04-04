# Configuration

## Struktur Konfigurasi

Konfigurasi dibagi dua lapis:
- Global policy: `include/app_config.h`
- Board profile (pin + sensor set): `include/profiles/profile_*.h`

Pemilihan profile lewat:
- `include/board_profile.h`
- `build_flags` di `platformio.ini`

Profile custom lokal (opsional):
- Profile template yang di-track git di-whitelist.
- File lain di `include/profiles/` di-ignore secara default, jadi varian lokal bisa disimpan dengan aman.
- Untuk custom cepat, salin profile yang sudah ada (misalnya `include/profiles/profile_esp32_c3.h`) ke file baru (misalnya `include/profiles/profile_humidity.h`), lalu edit lokal.

Override include berbasis macro (opsional):
- Definisikan `PROFILE_INCLUDE_PATH` di `build_flags` untuk meng-include path profile secara langsung.
- Contoh: `-DPROFILE_INCLUDE_PATH='"profiles/profile_humidity.h"'`
- Override ini punya prioritas tertinggi dan cocok untuk varian profile custom tanpa menambah macro selector baru.

## Profile Yang Tersedia

### `BOARD_PROFILE_ESP32_C3`

- DHT: aktif
- mmWave: aktif
- Servo output: nonaktif secara default
- Pin mmWave:
  - TX: GPIO3
  - RX: GPIO4

### `BOARD_PROFILE_ESP32_S3`

- DHT: aktif
- mmWave: nonaktif (template profile)
- Servo output: nonaktif secara default

### `BOARD_PROFILE_ESP32_CAM`

- Target board: AI Thinker ESP32-CAM (`esp32cam`)
- DHT: nonaktif (default)
- mmWave: nonaktif (default)
- Camera: aktif (mapping pin mengikuti referensi `esp32cam/pins.hpp` `AiThinker`)
- Servo output: nonaktif secara default (disiapkan untuk named group `camera_pan_tilt`)
- Pin camera:
  - D0: GPIO5
  - D1: GPIO18
  - D2: GPIO19
  - D3: GPIO21
  - D4: GPIO36
  - D5: GPIO39
  - D6: GPIO34
  - D7: GPIO35
  - XCLK: GPIO0
  - PCLK: GPIO22
  - VSYNC: GPIO25
  - HREF: GPIO23
  - SDA: GPIO26
  - SCL: GPIO27
  - RESET: -1
  - PWDN: GPIO32

## Powersave

Mode yang tersedia:
- Deep sleep (`POWERSAVE_SLEEP_MODE_DEEP`), default interval `10s`
- Light sleep (`POWERSAVE_SLEEP_MODE_LIGHT`), default interval `300ms`

Macro penting di `include/app_config.h`:
- `ENABLE_POWERSAVE`
- `POWERSAVE_SLEEP_MODE`
- `NODE_FORCE_ROLE_SLEEP_POLICY`
- `NODE_EFFECTIVE_SLEEP_MODE`
- `POWERSAVE_DEEP_SLEEP_SEC`
- `POWERSAVE_LIGHT_SLEEP_MS`

Kebijakan role paksa (default aktif):
- Jika ada module execute yang aktif (`SERVO_OUTPUT_ENABLED` atau `CAMERA_SENSOR_ENABLED`), mode efektif dipaksa ke light sleep.
- Jika node hanya expose module input, mode efektif dipaksa ke deep sleep.
- Kebijakan ini memakai `NODE_EFFECTIVE_SLEEP_MODE`, jadi override `POWERSAVE_SLEEP_MODE` dari env tidak mengubah perilaku runtime selama policy aktif.

## Sensor Filtering

mmWave filtering (default aktif):
- `MMWAVE_FILTER_ENABLED`
- `MMWAVE_PRESENCE_ON_CONSECUTIVE`
- `MMWAVE_PRESENCE_OFF_CONSECUTIVE`
- `MMWAVE_PRESENCE_OFF_HOLD_MS`
- `MMWAVE_DISTANCE_MEDIAN_WINDOW`

Default tuning mmWave (baseline profil ESP32-C3):
- `MMWAVE_READ_INTERVAL_MS=500`
- `MMWAVE_PRESENCE_ON_CONSECUTIVE=1`
- `MMWAVE_PRESENCE_OFF_CONSECUTIVE=2`
- `MMWAVE_PRESENCE_OFF_HOLD_MS=1500`
- `MMWAVE_DISTANCE_MEDIAN_WINDOW=3`

Battery analog filtering:
- `BATTERY_FILTER_ENABLED`
- `BATTERY_ADC_SAMPLES`
- `BATTERY_ADC_TRIM_PERCENT`
- `BATTERY_VOLTAGE_EMA_ALPHA`

## Servo Output (Opsional)

Kontrol servo bersifat command-driven dari master dan mendukung routing named logical group.

Macro penting di `include/app_config.h`:
- `SERVO_OUTPUT_ENABLED`
- `SERVO_GROUP_NAME`
- `SERVO_GROUP_CHANNEL_COUNT`
- `SERVO_MIN_DEG10`
- `SERVO_MAX_DEG10`
- `SERVO_DEFAULT_DEG10`

Mapping pin di board profile:
- `BOARD_PROFILE_SERVO_PIN_CH0`
- `BOARD_PROFILE_SERVO_PIN_CH1`

## WiFi Mode (Opsional)

Fitur WiFi STA untuk use-case lanjut (logging/dashboard/camera) dikontrol oleh macro:
- `ENABLE_WIFI_MODE` (default `0`)
- `WIFI_CONNECT_TIMEOUT_MS` (default `15000`)
- `WIFI_CLIENT_HOSTNAME` (default `DEVICE_NAME`)

Setting WebSocket gateway:
- `ENABLE_WEBSOCKET_GATEWAY` (default `1`, aktif hanya saat WiFi mode aktif dan sudah connected)
- `WEBSOCKET_SERVER_PORT` (default `81`)
- `WEBSOCKET_SERVER_PATH` (default `"/"`)
- `WEBSOCKET_MAX_HOOKS` (default `8`)
- `WEBSOCKET_CAMERA_DEFAULT_WIDTH` (default `320`)
- `WEBSOCKET_CAMERA_DEFAULT_HEIGHT` (default `240`)
- `WEBSOCKET_CAMERA_DEFAULT_FORMAT` (default `"jpg"`)
- `WEBSOCKET_CAMERA_XCLK_HZ` (default `20000000`)
- `WEBSOCKET_CAMERA_JPEG_QUALITY` (default `12`)
- `WEBSOCKET_CAMERA_FB_COUNT` (default `2`)
- `WEBSOCKET_STREAM_MAX_CLIENTS` (default `4`)
- `WEBSOCKET_STREAM_DEFAULT_INTERVAL_MS` (default `500`)
- `WEBSOCKET_STREAM_MIN_INTERVAL_MS` (default `120`)

Perilaku:
- Node tidak akan connect WiFi otomatis saat boot.
- Node hanya akan connect WiFi jika:
  1. `ENABLE_WIFI_MODE == 1`
  2. master mengirim command kredensial WiFi.
- Jika master tidak mengirim kredensial, node skip koneksi WiFi.
- Saat koneksi berhasil, node publish hostname WiFi dan mDNS di `<hostname>.local`.
- Server WebSocket hanya start setelah WiFi connected dan menyediakan hook berbasis `type` untuk request multipurpose.

Kebijakan control/discovery:
- Kontrol output dan discovery module diprioritaskan pada kontrak command/state ESP-NOW.
- Jalur WebSocket dipertahankan terutama untuk kompatibilitas payload besar (misalnya frame/stream camera).

Env build siap pakai:
- `esp32-c3-super-mini-wifi` (set `ENABLE_WIFI_MODE=1`)
- `esp32-cam-ai-thinker` (set `BOARD_PROFILE_ESP32_CAM` dan `ENABLE_WIFI_MODE=1`)

## Behavior Boot

Sebelum coba link ke master:
1. Boot init semua sensor aktif.
2. Ambil 1 sample awal dari setiap module.
3. Tulis sample ke log.

Jika master tidak terhubung, flow powersave tetap lanjut ke cycle sleep.

Kebijakan sleep gating (powersave):
- Node hanya boleh sleep saat tidak ada task aktif dari master.
- Node hanya boleh sleep setelah aktivitas master idle minimal `NODE_SLEEP_IDLE_THRESHOLD_MS` (default `5000ms`).
- Traffic keepalive (misalnya heartbeat) tidak dianggap sebagai aktivitas task jangka panjang.

English version: [configuration.md](configuration.md)
