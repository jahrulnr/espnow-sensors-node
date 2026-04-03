# Kontrak WebSocket (Node Gateway)

Dokumen ini mendefinisikan kontrak WebSocket multipurpose untuk client yang berinteraksi dengan node lewat WiFi.

Catatan scope control/discovery:
- Kontrak utama untuk control dan discovery module ada di ESP-NOW.
- WebSocket gateway diprioritaskan untuk kompatibilitas payload besar (misalnya frame/stream camera).

## Ketersediaan

WebSocket gateway tersedia hanya jika:
- `ENABLE_WIFI_MODE == 1`
- Node sudah connected ke WiFi setelah menerima command `WifiCredentials` dari master
- `ENABLE_WEBSOCKET_GATEWAY == 1`

Endpoint default:
- `ws://<node-ip>:81/`

Port bisa diubah lewat `WEBSOCKET_SERVER_PORT`.

## Format Envelope

Semua pesan JSON memakai envelope berikut:

```json
{"type":"<hook-type>","data":{...}}
```

Contoh:

Request:

```json
{"type":"camera","data":{"state":"specs"}}
```

Response:

```json
{"type":"camera","data":{"state":"specs","width":320,"height":240,"format":"jpg"}}
```

## Model Routing

- Request dirouting berdasarkan `type` ke hook yang terdaftar.
- `type` yang tidak dikenal akan dibalas error terstruktur.
- Parsing dan penulisan JSON menggunakan `SpiJsonDocument`.

## Hook Bawaan

### `system`

State request yang didukung:
- `{"type":"system","data":{"state":"ping"}}` -> envelope pong
- `{"type":"system","data":{"state":"hooks"}}` -> daftar type hook aktif

### `camera`

State request yang didukung:
- `{"type":"camera","data":{"state":"specs"}}` -> mengembalikan envelope capability camera
- `{"type":"camera","data":{"state":"config_get"}}` -> mengembalikan config runtime camera saat ini (frameSize, xclk, mirror/flip, tuning gambar)
- `{"type":"camera","data":{"state":"config_set","frameSize":6,"xclkHz":20000000,"hmirror":false,"vflip":true,"quality":12,"brightness":0,"contrast":0,"saturation":0}}` -> memperbarui config runtime camera lalu mengembalikan nilai terbaru
- `{"type":"camera","data":{"state":"frame"}}` -> capture 1 frame lalu kirim metadata JSON + payload JPEG binary
- `{"type":"camera","data":{"state":"stream","action":"start","intervalMs":500}}` -> mulai streaming frame periodik untuk client ini
- `{"type":"camera","data":{"state":"stream","action":"stop"}}` -> stop streaming frame periodik untuk client ini

## Format Response Error

Response error mengikuti bentuk berikut:

```json
{
  "type":"camera",
  "data":{
    "ok":false,
    "error":{
      "code":"camera_disabled",
      "message":"Camera is not enabled in this build profile"
    }
  }
}
```

## Response Binary

Response bisa berupa JSON atau binary tergantung implementasi hook.

Pendekatan yang disarankan untuk binary:
- Kirim dulu envelope metadata JSON (type, format, dimensi, sequence).
- Kirim payload frame binary pada pesan WebSocket binary berikutnya.

## Sumber Kontrak di Kode

- `src/app/network/websocket_gateway.h`
- `src/app/network/websocket_gateway.cpp`
- `src/app/network/wifi_manager.cpp`

Panduan implementasi hook: [add-websocket-hook_id.md](add-websocket-hook_id.md)

English version: [websocket-contract.md](websocket-contract.md)
