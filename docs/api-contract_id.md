# API Contract (Node <-> Master)

Dokumen ini adalah kontrak integrasi untuk implementasi master agar parsing dan flow tidak miss.

## Versioning

Ada 3 level versi yang harus dicek:
- Transport frame version: `PROTOCOL_VERSION = 1`
- Binary state header version: `state_binary::kVersion = 1`
- Feature contract version: `FeaturesState.contractVersion = 1`

Jika versi tidak cocok, master sebaiknya drop frame/payload tersebut.

## Transport Frame

Semua data dikirim dalam frame berikut (`packed`):

```c
struct PacketHeader {
  uint8_t version;      // PROTOCOL_VERSION
  uint8_t type;         // PacketType
  uint16_t sequence;    // per-node incrementing
  uint32_t timestampMs; // millis() dari node/master pengirim
};

struct Frame {
  PacketHeader header;
  uint8_t payloadSize;              // 0..200
  uint8_t payload[MAX_PAYLOAD_SIZE];
};
```

Konstanta:
- `MAX_PAYLOAD_SIZE = 200`
- `DEFAULT_CHANNEL = 1`

## PacketType Contract

`PacketType`:
- `1 = HELLO`
- `2 = HEARTBEAT`
- `3 = COMMAND`
- `4 = STATE`

Arah yang dipakai saat ini:
- Master -> Node:
  - `HELLO`, `HEARTBEAT`, `COMMAND`, (opsional `STATE` tipe `MasterNet`)
- Node -> Master:
  - `HELLO`, `STATE`

## Discovery Dan Linking

Perilaku penting di node:
- Node scan channel `1..13` setiap `300ms`.
- Sender yang belum dikenal hanya diterima kalau kirim `HELLO` atau `HEARTBEAT`.
- Sender unknown yang kirim tipe lain akan diabaikan.
- Timeout master di node: `12000ms` tanpa traffic.
- Node kirim `HELLO` periodik ke master known tiap `7000ms`.

Implikasi ke master:
- Master wajib broadcast/unicast `HELLO` atau `HEARTBEAT` periodik agar terdeteksi.
- Disarankan interval heartbeat jauh di bawah 12 detik (contoh `1s`).

## STATE Payload Contract (Binary)

Mayoritas payload `STATE` adalah binary state dengan header internal:

```c
struct Header {
  uint8_t magic;    // 0xB1
  uint8_t version;  // 1
  uint8_t type;     // state_binary::Type
  uint8_t reserved; // 0
};
```

Validasi minimum di master:
1. `payloadSize >= sizeof(Header)`
2. `magic == 0xB1`
3. `version == 1`
4. Ukuran payload cocok dengan struct type yang di-parse

## state_binary::Type Yang Aktif Dipakai

- `1 = Identity`
- `2 = Sensor` (DHT)
- `5 = MasterNet` (dari master ke node)
- `6 = SlaveAlive`
- `9 = Features`
- `10 = IdentityReq` (COMMAND payload)
- `11 = Mmwave`
- `12 = WifiCredentials` (COMMAND payload)

## Payload Structures Dan Semantik

### Identity (`Type=1`)

```c
struct IdentityState {
  Header header;
  char id[24]; // null-terminated jika muat, bisa terpotong
};
```

### Sensor DHT (`Type=2`)

```c
struct SensorState {
  Header header;
  int16_t temperature10; // suhu C x 10
  uint16_t humidity10;   // RH % x 10
};
```

Decode:
- `temperatureC = temperature10 / 10.0`
- `humidityPercent = humidity10 / 10.0`

### MasterNet (`Type=5`)

```c
struct MasterNetState {
  Header header;
  uint8_t online;  // 0/1
  uint8_t channel; // channel info dari master
};
```

### SlaveAlive (`Type=6`)

```c
struct SlaveAliveState {
  Header header;
};
```

### Features (`Type=9`)

```c
struct FeaturesState {
  Header header;
  uint32_t featureBits;
  uint16_t contractVersion; // saat ini = 1
  uint16_t reserved;
};
```

Feature bits yang dipakai node:
- `bit0 (1<<0) = FeatureIdentity`
- `bit1 (1<<1) = FeatureSensor` (DHT)
- `bit7 (1<<7) = FeatureMmwave`
- `bit8 (1<<8) = FeatureWifiSta` (kapabilitas WiFi command, aktif jika build enable WiFi mode)

Master harus treat bit lain sebagai unknown/forward-compatible.

### Mmwave (`Type=11`)

```c
struct MmwaveState {
  Header header;
  uint8_t detected;    // 0/1
  uint8_t hasDistance; // 0/1
  uint16_t distanceCm;
  uint16_t frameCount;
  uint16_t byteCount;
};
```

## COMMAND Contract

Payload untuk `PacketType::COMMAND` saat ini yang diproses:

### IdentityReq (`Type=10`)

```c
struct IdentityReqCommand {
  Header header;
};
```

Respons node:
1. kirim `IdentityState`
2. kirim `FeaturesState`

Command selain `IdentityReq` dan `WifiCredentials` saat ini diabaikan.

### WifiCredentials (`Type=12`)

```c
struct WifiCredentialsCommand {
  Header header;
  char ssid[32];
  char password[64];
};
```

Perilaku node:
- Jika `ENABLE_WIFI_MODE == 1`: node queue connect ke SSID/password yang diterima.
- Jika `ENABLE_WIFI_MODE == 0`: command diabaikan.
- Jika master tidak pernah kirim command ini: node tidak connect WiFi.
- Hostname WiFi tidak dikirim di command ini; hostname ditentukan lokal via config `WIFI_CLIENT_HOSTNAME`.

Catatan encoding field:
- `ssid`/`password` boleh null-terminated atau buffer terisi parsial.
- Node akan memotong panjang ke kapasitas field (`32`/`64`) bila lebih panjang.

## Runtime Sequence (Praktis Untuk Master)

Urutan umum setelah node linked:
1. Node kirim `IdentityState` dan `FeaturesState`.
2. Node kirim sample sensor (`SensorState`, `MmwaveState`) sesuai interval/module.
3. Master bisa kirim `IdentityReq` kapan pun untuk re-sync metadata node.
4. Master kirim `HEARTBEAT` periodik untuk menjaga link.

Pada mode powersave:
- Tiap wake cycle node menunggu link sampai timeout.
- Jika linked: kirim identity/features lalu sample boot masing-masing module aktif.
- Jika tidak linked: node sleep lagi.

## Parsing Rules Yang Disarankan Di Master

1. Validasi `Frame` (`payloadSize <= 200`, length konsisten).
2. Cek `PacketHeader.version == 1`.
3. Route by `PacketType`.
4. Untuk `STATE` binary:
   - validasi magic/version/type/size
   - decode sesuai tabel struct di atas
5. Unknown type/unknown feature bit: ignore, jangan crash.

## Sumber Kontrak Di Kode

- `src/app/espnow/protocol.h`
- `src/app/espnow/state_binary.h`
- `src/app/espnow/slave.cpp`
- `src/app/espnow/contract_checks.cpp`

Kontrak API WebSocket didokumentasikan terpisah di [websocket-contract_id.md](websocket-contract_id.md).
