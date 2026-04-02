# Add WebSocket Hook

Panduan singkat menambah WebSocket hook modular tanpa hardcode behavior di layer transport gateway.

Lihat index docs: [README_id.md](README_id.md)

## 1) Buat Kelas Hook

Buat file di `src/app/network/hooks/` dan implement `IWebsocketHook` dari:

- `src/app/network/hooks/websocket_hook.h`

Method yang wajib:

- `type()`
- `begin(WebsocketGateway&)`
- `loop(WebsocketGateway&)`
- `handleRequest(WebsocketGateway&, uint8_t clientId, JsonVariantConst requestData)`
- `onClientDisconnected(WebsocketGateway&, uint8_t clientId)`

## 2) Simpan State di Module Hook

Simpan state runtime khusus hook di kelas hook milikmu, bukan di `websocket_gateway`.

Contoh state yang tepat:

- slot stream client
- counter request atau sequence ID
- flag readiness fitur

## 3) Register Hook di Hook Manager

Daftarkan hook baru di:

- `src/app/network/hooks/websocket_hook_manager.cpp`

Flow registrasi saat ini:

- buat static instance hook
- panggil `registerHook(...)` di `ensureRegistryInitialized()`

## 4) Tangani Envelope Request Konsisten

Format envelope request masuk:

```json
{"type":"<hook-type>","data":{...}}
```

Di `handleRequest(...)`, parse `requestData` lalu kirim response lewat helper `WebsocketGateway`:

- `sendEnvelope(...)` untuk response JSON
- `sendBinary(...)` untuk payload binary
- `sendError(...)` untuk error terstruktur

## 5) Gunakan SpiJsonDocument untuk JSON

Untuk parsing/penulisan JSON, gunakan:

- `SpiJsonDocument`
- include dari `lib/SpiJsonDocument/src/SpiJsonDocument.h`

Ini menjaga alokasi memori tetap sejalan dengan strategi memory project.

## 6) Tambah Macro Konfigurasi (Opsional)

Jika hook butuh default yang bisa di-tune, tambahkan macro di:

- `include/app_config.h`

Gunakan pola default `#ifndef` supaya mudah dioverride.

## 7) Update Docs Kontrak WebSocket

Kalau hook baru menambah state request/payload format, update:

- `docs/websocket-contract.md`
- `docs/websocket-contract_id.md`

## 8) Validasi Build

Minimal jalankan build WiFi-enabled:

```bash
pio run -e dev
```

Untuk hook terkait kamera, validasi juga:

```bash
pio run -e esp32-cam-ai-thinker
```

## 9) Smoke Test Request

Contoh request lewat WebSocket:

```json
{"type":"system","data":{"state":"hooks"}}
```

Pakai ini untuk memastikan hook baru muncul di daftar hook runtime.

English version: [add-websocket-hook.md](add-websocket-hook.md)
