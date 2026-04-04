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
- Node scan channel `1..13` setiap `1500ms` secara default (`NODE_SCAN_CHANNEL_DWELL_MS`).
- Node memakai array cache channel master berbasis RTC+NVS untuk percepat reacquire saat wake.
- Node tetap menjalankan full scan channel berdasarkan TTL cache (`NODE_MASTER_CACHE_FULLSCAN_TTL_MS`, default `30000ms`).
- Cache hanya dianggap invalid jika gagal link ke master lebih dari `5` wake cycle berturut-turut (`NODE_MASTER_CACHE_INVALID_AFTER_WAKE_MISS`).
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
- `13 = ServoControl` (COMMAND payload)
- `14 = ServoAck` (STATE payload)
- `15 = ModuleListReq` (COMMAND payload)
- `16 = ModuleInfo` (STATE payload)
- `17 = CameraCapture` (STATE payload)
- `18 = WifiKeyExchange` (STATE payload)
- `19 = WifiCredentialsSecure` (COMMAND payload)
- `20 = MmwaveRangeConfig` (COMMAND payload)
- `21 = WifiWsEndpoint` (STATE payload)

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
- `bit9 (1<<9) = FeatureActuationServo`
- `bit4 (1<<4) = FeatureCameraJpeg`
- `bit5 (1<<5) = FeatureCameraStream`

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
  uint8_t targetState; // ekstensi opsional, 0=none,1=moving,2=stationary,3=both
  uint8_t reportType;  // ekstensi opsional dari periodic report LD2410
};

`targetState` dan `reportType` adalah field ekstensi forward-compatible.
Master lama bisa mengabaikan field ini dan tetap mem-parse field dasar (`detected/hasDistance/distanceCm/frameCount/byteCount`).

### ServoAck (`Type=14`)

```c
struct ServoAckState {
  Header header;
  uint8_t ok;       // 0/1
  uint8_t status;   // 0=ok, 1=disabled, 2=invalidGroup, 3=invalidChannel, 4=invalidValue, 5=driverError
  char group[16];   // named logical group
  uint8_t channel;  // channel index di dalam group
  uint16_t targetDeg10;
  uint16_t appliedDeg10;
  uint32_t timestampMs;
};
```

### WifiKeyExchange (`Type=18`)

```c
struct WifiKeyExchangeState {
  Header header;
  uint32_t keyId;
  uint8_t curve;            // saat ini 1 = secp256r1
  uint8_t publicKeySize;    // saat ini 33
  uint8_t publicKey[33];    // compressed EC public key
};
```

Panduan parsing di master:
- Treat nilai `curve` yang unknown sebagai unsupported/forward-compatible dan lewati secure provisioning.
- Wajibkan `publicKeySize == 33` sebelum key dipakai untuk enkripsi kredensial aman.
- `keyId` mengidentifikasi material key aktif node dan ikut mengikat anti-replay.

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
3. kirim `WifiKeyExchangeState` saat material key secure channel tersedia

Command selain `IdentityReq`, `WifiCredentials`, `WifiCredentialsSecure`, `MmwaveRangeConfig`, `ServoControl`, dan `ModuleListReq` saat ini diabaikan.

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

### WifiCredentialsSecure (`Type=19`)

```c
struct WifiCredentialsSecureCommand {
  Header header;
  uint32_t keyId;
  uint32_t counter;
  uint8_t ephemeralKeySize;     // saat ini 33
  uint8_t ephemeralPublicKey[33];
  uint8_t nonce[12];
  uint8_t ciphertext[98];
  uint8_t tag[16];
};
```

Perilaku node:
- Validasi `keyId` terhadap identifier keypair lokal yang aktif.
- Validasi `counter` harus naik ketat (anti-replay).
- Derivasi session key via ECDH shared secret + SHA-256 KDF input (`shared`, `keyId`, `counter`, label `fh-wifi-v1`).
- Dekripsi plaintext kredensial fixed-size dengan AES-GCM (AAD mengikat type/keyId/counter).
- Jika dekripsi + validasi sukses: queue request connect WiFi menggunakan SSID/password hasil dekripsi.
- Jika validasi/dekripsi gagal: command ditolak aman dan tidak ada connect request yang di-queue.

### MmwaveRangeConfig (`Type=20`)

```c
struct MmwaveRangeConfigCommand {
  Header header;
  uint16_t maxDistanceCm; // 0 menonaktifkan batas jarak, selain itu batas maksimum cm
  uint8_t persistToNvs;   // 0/1
  uint8_t reserved0;
};
```

Perilaku node:
- Menerapkan filter jarak maksimum mmWave pada pemrosesan sample runtime.
- Jika `maxDistanceCm == 0`, batas jangkauan dinonaktifkan.
- Jika `persistToNvs == 1`, nilai yang diterapkan disimpan ke NVS dan dipakai ulang saat boot berikutnya.
- Jika `persistToNvs == 0`, nilai hanya berlaku saat runtime dan default profile/NVS sebelumnya tidak diubah.

### WifiWsEndpoint (`Type=21`)

```c
struct WifiWsEndpointState {
  Header header;
  uint8_t connected;  // 0/1
  uint8_t ip[4];      // byte IPv4, 0.0.0.0 saat disconnected
  uint16_t port;      // port websocket (default 81)
  char path[24];      // path websocket (default "/")
  char hostname[32];  // hostname aktif (fallback mdns)
};
```

Perilaku node:
- Mengirim state ini pada alur respons `IdentityReq`.
- Mengirim ulang state ini saat endpoint WiFi berubah (connect/disconnect/perubahan IP).
- Mengirim refresh periodik saat connected agar master/FE bisa memulihkan endpoint setelah gangguan link sementara.

### ServoControl (`Type=13`)

```c
struct ServoControlCommand {
  Header header;
  char group[16];   // named logical group, contoh: "camera_pan_tilt"
  uint8_t channel;  // index channel servo dalam group
  uint16_t targetDeg10;
  uint16_t transitionMs; // metadata untuk policy transisi/smoothing
};
```

Perilaku node:
- Mendelegasikan command ke actuator manager modular.
- Mengirim satu `ServoAckState` sebagai payload `PacketType::STATE`.
- Jika validasi gagal (group/channel/range/driver), `ok=0` dan `status` berisi penyebab.

### ModuleListReq (`Type=15`)

```c
struct ModuleListReqCommand {
  Header header;
};
```

Perilaku node:
- Enumerasi module yang tersedia dari manager sensing dan actuation.
- Mengirim satu atau lebih `ModuleInfo`.
- Urutan deterministik: semua sensor dulu, lalu actuator.

### ModuleInfo (`Type=16`)

```c
struct ModuleInfoState {
  Header header;
  uint8_t index;      // index 0-based di sequence respons saat ini
  uint8_t total;      // total entry pada sequence respons saat ini
  uint8_t domain;     // 1=sensor, 2=actuator
  uint8_t reserved0;
  uint32_t featureBits;
  char id[16];        // id module (misalnya "dht", "mmwave", "camera", "servo")
};
```

Panduan parsing di master:
- Kelompokkan entry berdasarkan konteks sequence dan nilai `total`.
- Nilai `domain` unknown harus diperlakukan forward-compatible dan diabaikan dengan aman.
- `featureBits` dapat berisi lebih dari satu bit untuk satu module (contoh camera JPEG + stream).

## Runtime Sequence (Praktis Untuk Master)

Urutan umum setelah node linked:
1. Node kirim `IdentityState` dan `FeaturesState`.
2. Node kirim `WifiKeyExchangeState` (saat secure channel siap).
3. Node kirim sample sensor (`SensorState`, `MmwaveState`) sesuai interval/module.
4. Master bisa kirim `IdentityReq` kapan pun untuk re-sync metadata node (termasuk secure key exchange state).
5. Master bisa kirim `WifiCredentialsSecure` (disarankan) atau `WifiCredentials` (kompatibilitas) untuk meminta node connect WiFi.
6. Master bisa kirim `ServoControl` dan menerima `ServoAck` sebagai respons runtime state.
7. Master bisa kirim `ModuleListReq` untuk discovery module tersedia via ESP-NOW.
8. Master kirim `HEARTBEAT` periodik untuk menjaga link.

Pada mode powersave:
- Tiap wake cycle node menunggu link sampai timeout.
- Jika linked: kirim identity/features lalu sample boot masing-masing module aktif.
- Sebelum sleep, node menunggu sampai tidak ada task aktif dari master dan aktivitas master idle selama `NODE_SLEEP_IDLE_THRESHOLD_MS`.
- Jika tidak linked: node sleep lagi.

## Parsing Rules Yang Disarankan Di Master

1. Validasi `Frame` (`payloadSize <= 200`, length konsisten).
2. Cek `PacketHeader.version == 1`.
3. Route by `PacketType`.
4. Untuk `STATE` binary:
   - validasi magic/version/type/size
   - decode sesuai tabel struct di atas
5. Type tidak dikenal atau feature bit tidak dikenal: abaikan, jangan crash.

## Sumber Kontrak Di Kode

- `src/app/espnow/protocol.h`
- `src/app/espnow/state_binary.h`
- `src/app/espnow/slave.cpp`
- `src/app/espnow/contract_checks.cpp`

Kontrak API WebSocket didokumentasikan terpisah di [websocket-contract_id.md](websocket-contract_id.md).

English version: [api-contract.md](api-contract.md)
