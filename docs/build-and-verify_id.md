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
- Pada mode powersave:
  - Deep sleep sesuai interval default `10s`
  - Light sleep sesuai interval default `300ms`
- Contract payload dijaga oleh static check di:
  - `src/app/espnow/contract_checks.cpp`

## Validasi WiFi Command (Opsional)

Prasyarat:
- Build dengan `ENABLE_WIFI_MODE=1`.
- Master mengirim `PacketType::COMMAND` dengan payload `Type=WifiCredentials`.

Expected:
- Node log bahwa request connect di-queue.
- Node mencoba konek ke SSID yang dikirim.
- Saat sukses, log menampilkan hostname dan endpoint mDNS `<hostname>.local`.
- Jika tidak ada command kredensial dari master, node tidak connect WiFi.

## Validasi WebSocket (Opsional)

Prasyarat:
- Flow command WiFi sudah membuat node connected.

Expected:
- Log menampilkan websocket gateway aktif pada port yang dikonfigurasi.
- Kirim request JSON dengan envelope `{"type":"system","data":{"state":"hooks"}}` mendapat response envelope JSON.
- Kirim `{"type":"camera","data":{"state":"specs"}}` mendapat response JSON spesifikasi camera.
