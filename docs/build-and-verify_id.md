# Build And Verify

## Build Per Environment

Build C3 default:

```bash
pio run -e esp32-c3-super-mini
```

Build C3 deep sleep:

```bash
pio run -e esp32-c3-super-mini-deep
```

Build C3 light sleep:

```bash
pio run -e esp32-c3-super-mini-light
```

Build S3 template:

```bash
pio run -e esp32-s3-devkitc1-n16r8
```

Build C3 dengan WiFi mode enable:

```bash
pio run -e esp32-c3-super-mini-wifi
```

Build profile AI Thinker ESP32-CAM:

```bash
pio run -e esp32-cam-ai-thinker
```

Build matrix sekaligus:

```bash
pio run -e esp32-c3-super-mini -e esp32-c3-super-mini-deep -e esp32-c3-super-mini-light -e esp32-s3-devkitc1-n16r8
```

## Validasi Yang Wajib

- Build matrix lintas profile harus sukses.
- Boot log harus menampilkan 1 sample awal per module aktif.
- Boot log sebaiknya tidak menampilkan kegagalan init actuator yang tidak diharapkan saat output memang dinonaktifkan di profile.
- Pada mode powersave:
  - Deep sleep sesuai interval default `10s`
  - Light sleep sesuai interval default `300ms`
- Contract payload dijaga oleh static check di:
  - `src/app/espnow/contract_checks.cpp`

## Validasi ESP-NOW Control + Discovery (Disarankan)

Prasyarat:
- Master sudah linked ke node (flow `HELLO` / `HEARTBEAT` sehat).

Validasi discovery:
- Kirim `PacketType::COMMAND` dengan `Type=IdentityReq` lalu pastikan node membalas `IdentityState` dan `FeaturesState`.
- Kirim `PacketType::COMMAND` dengan `Type=ModuleListReq` lalu pastikan node membalas satu atau lebih `ModuleInfo`.
- Pastikan `ModuleInfo.index` naik dari `0` dan `ModuleInfo.total` konsisten dalam satu sequence.

Validasi control (saat servo output di-enable di profile):
- Kirim `PacketType::COMMAND` dengan `Type=ServoControl`.
- Pastikan node membalas state `ServoAck`.
- Verifikasi perilaku error untuk group/channel/range invalid: `ok=0` dan `status` sesuai.

## Validasi WiFi Command (Opsional)

Prasyarat:
- Build dengan `ENABLE_WIFI_MODE=1`.
- Master mengirim `PacketType::COMMAND` dengan payload `Type=WifiCredentials`.

Expected:
- Node log bahwa request connect di-queue.
- Node mencoba konek ke SSID yang dikirim.
- Saat sukses, log menampilkan hostname dan endpoint mDNS `<hostname>.local`.
- Jika tidak ada command kredensial dari master, node tidak connect WiFi.

## Validasi Secure WiFi Provisioning (Opsional)

Prasyarat:
- Build dengan `ENABLE_WIFI_MODE=1`.
- Master mendukung `WifiKeyExchange` (`Type=18`) dan `WifiCredentialsSecure` (`Type=19`).

Expected discovery behavior:
- Setelah master mengirim `IdentityReq`, node mengirim `IdentityState`, `FeaturesState`, dan `WifiKeyExchange`.
- `WifiKeyExchange.publicKeySize` harus `33` dan `curve` harus `1` (secp256r1).

Expected secure command behavior:
- Saat menerima `Type=19` yang valid, node mendekripsi kredensial lalu queue request connect WiFi.
- Saat payload `Type=19` yang sama di-replay (`counter` tidak berubah), node menolak command (anti-replay) dan tidak queue connect.
- Saat `keyId` tidak cocok dengan state key lokal, node menolak command dengan aman.

## Validasi Override Jangkauan mmWave (Opsional)

Prasyarat:
- Module mmWave aktif dan mengirim report valid.
- Master dapat mengirim `PacketType::COMMAND` dengan `Type=MmwaveRangeConfig (20)`.

Expected:
- Payload command dengan `maxDistanceCm` dan `persistToNvs` diterima oleh command hook node.
- Jika `maxDistanceCm` lebih kecil dari jarak target saat ini, output node harus ter-clamp menjadi no-detection (`targetState=0`, `detected=0`).
- Jika `persistToNvs=1`, range yang dikonfigurasi tetap aktif setelah reboot.
- Jika `persistToNvs=0`, override runtime tidak menimpa setting persisted/default.

## Validasi WebSocket (Opsional)

Prasyarat:
- Flow command WiFi sudah membuat node connected.

Expected:
- Log menampilkan websocket gateway aktif pada port yang dikonfigurasi.
- Kirim request JSON dengan envelope `{"type":"system","data":{"state":"hooks"}}` mendapat response envelope JSON.
- Kirim `{"type":"camera","data":{"state":"specs"}}` mendapat response JSON spesifikasi camera.

English version: [build-and-verify.md](build-and-verify.md)
