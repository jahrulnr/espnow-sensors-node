# API Contract (Node <-> Master)

This document is the integration contract for master implementations so parsing and runtime flow stay aligned.

## Versioning

There are 3 version levels to validate:
- Transport frame version: `PROTOCOL_VERSION = 1`
- Binary state header version: `state_binary::kVersion = 1`
- Feature contract version: `FeaturesState.contractVersion = 1`

If versions do not match, master should drop the frame/payload.

## Transport Frame

All data is sent in the following frame (`packed`):

```c
struct PacketHeader {
  uint8_t version;      // PROTOCOL_VERSION
  uint8_t type;         // PacketType
  uint16_t sequence;    // per-node incrementing
  uint32_t timestampMs; // millis() from sender node/master
};

struct Frame {
  PacketHeader header;
  uint8_t payloadSize;              // 0..200
  uint8_t payload[MAX_PAYLOAD_SIZE];
};
```

Constants:
- `MAX_PAYLOAD_SIZE = 200`
- `DEFAULT_CHANNEL = 1`

## PacketType Contract

`PacketType`:
- `1 = HELLO`
- `2 = HEARTBEAT`
- `3 = COMMAND`
- `4 = STATE`

Current direction usage:
- Master -> Node:
  - `HELLO`, `HEARTBEAT`, `COMMAND`, (optional `STATE` with `MasterNet` type)
- Node -> Master:
  - `HELLO`, `STATE`

## Discovery and Linking

Important node behavior:
- Node scans channels `1..13` every `1500ms` by default (`NODE_SCAN_CHANNEL_DWELL_MS`).
- Node uses RTC+NVS cached master-channel array for faster wake reacquisition.
- Node runs full channel sweep by cache TTL (`NODE_MASTER_CACHE_FULLSCAN_TTL_MS`, default `30000ms`).
- Cache is invalidated only after more than `5` consecutive wake cycles without master link (`NODE_MASTER_CACHE_INVALID_AFTER_WAKE_MISS`).
- Unknown sender is accepted only if it sends `HELLO` or `HEARTBEAT`.
- Unknown sender packets with other types are ignored.
- Master timeout on node: `12000ms` without traffic.
- Node sends periodic `HELLO` to known master every `7000ms`.

Implications for master:
- Master must periodically broadcast/unicast `HELLO` or `HEARTBEAT` so node can detect it.
- Heartbeat interval should be well below 12 seconds (for example `1s`).

## STATE Payload Contract (Binary)

Most `STATE` payloads use a binary-state internal header:

```c
struct Header {
  uint8_t magic;    // 0xB1
  uint8_t version;  // 1
  uint8_t type;     // state_binary::Type
  uint8_t reserved; // 0
};
```

Minimum master validation:
1. `payloadSize >= sizeof(Header)`
2. `magic == 0xB1`
3. `version == 1`
4. Payload size must match parsed struct type

## Active state_binary::Type Values

- `1 = Identity`
- `2 = Sensor` (DHT)
- `5 = MasterNet` (from master to node)
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

## Payload Structures and Semantics

### Identity (`Type=1`)

```c
struct IdentityState {
  Header header;
  char id[24]; // null-terminated when it fits, may be truncated
};
```

### Sensor DHT (`Type=2`)

```c
struct SensorState {
  Header header;
  int16_t temperature10; // temperature C x 10
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
  uint8_t channel; // channel info from master
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
  uint16_t contractVersion; // currently = 1
  uint16_t reserved;
};
```

Feature bits used by node:
- `bit0 (1<<0) = FeatureIdentity`
- `bit1 (1<<1) = FeatureSensor` (DHT)
- `bit7 (1<<7) = FeatureMmwave`
- `bit8 (1<<8) = FeatureWifiSta` (WiFi command capability, enabled when WiFi mode build is enabled)
- `bit9 (1<<9) = FeatureActuationServo`
- `bit4 (1<<4) = FeatureCameraJpeg`
- `bit5 (1<<5) = FeatureCameraStream`

Master should treat other bits as unknown/forward-compatible.

### Mmwave (`Type=11`)

```c
struct MmwaveState {
  Header header;
  uint8_t detected;    // 0/1
  uint8_t hasDistance; // 0/1
  uint16_t distanceCm;
  uint16_t frameCount;
  uint16_t byteCount;
  uint8_t targetState; // optional ext, 0=none,1=moving,2=stationary,3=both
  uint8_t reportType;  // optional ext from LD2410 periodic report
};

`targetState` and `reportType` are optional forward-compatible extension fields.
Older masters can ignore them and still parse base fields (`detected/hasDistance/distanceCm/frameCount/byteCount`).

### ServoAck (`Type=14`)

```c
struct ServoAckState {
  Header header;
  uint8_t ok;       // 0/1
  uint8_t status;   // 0=ok, 1=disabled, 2=invalidGroup, 3=invalidChannel, 4=invalidValue, 5=driverError
  char group[16];   // named logical group
  uint8_t channel;  // channel index inside group
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
  uint8_t curve;            // currently 1 = secp256r1
  uint8_t publicKeySize;    // currently 33
  uint8_t publicKey[33];    // compressed EC public key
};
```

Master parsing guidance:
- Treat unknown `curve` values as unsupported/forward-compatible and skip secure provisioning.
- Require `publicKeySize == 33` before using the key for secure credential encryption.
- `keyId` identifies the active node key material and is part of anti-replay binding.

## COMMAND Contract

Currently processed payloads for `PacketType::COMMAND`:

### IdentityReq (`Type=10`)

```c
struct IdentityReqCommand {
  Header header;
};
```

Node response:
1. sends `IdentityState`
2. sends `FeaturesState`
3. sends `WifiKeyExchangeState` when secure channel key material is available

Commands other than `IdentityReq`, `WifiCredentials`, `WifiCredentialsSecure`, `MmwaveRangeConfig`, `ServoControl`, and `ModuleListReq` are currently ignored.

### WifiCredentials (`Type=12`)

```c
struct WifiCredentialsCommand {
  Header header;
  char ssid[32];
  char password[64];
};
```

Node behavior:
- If `ENABLE_WIFI_MODE == 1`: node queues connection to the received SSID/password.
- If `ENABLE_WIFI_MODE == 0`: command is ignored.
- If master never sends this command: node does not connect to WiFi.
- WiFi hostname is not sent in this command; hostname is local config via `WIFI_CLIENT_HOSTNAME`.

Field encoding notes:
- `ssid`/`password` may be null-terminated or partially filled buffers.
- Node truncates length to field capacities (`32`/`64`) when longer.

### WifiCredentialsSecure (`Type=19`)

```c
struct WifiCredentialsSecureCommand {
  Header header;
  uint32_t keyId;
  uint32_t counter;
  uint8_t ephemeralKeySize;     // currently 33
  uint8_t ephemeralPublicKey[33];
  uint8_t nonce[12];
  uint8_t ciphertext[98];
  uint8_t tag[16];
};
```

Node behavior:
- Validates `keyId` against current local keypair identifier.
- Validates `counter` is strictly increasing (anti-replay).
- Derives session key via ECDH shared secret + SHA-256 KDF inputs (`shared`, `keyId`, `counter`, label `fh-wifi-v1`).
- Decrypts fixed-size plaintext credentials using AES-GCM (AAD binds type/keyId/counter).
- If decrypt and validation pass: queues WiFi connect request using recovered SSID/password.
- If validation/decrypt fails: command is rejected safely and no connect is queued.

### MmwaveRangeConfig (`Type=20`)

```c
struct MmwaveRangeConfigCommand {
  Header header;
  uint16_t maxDistanceCm; // 0 disables range limit, otherwise max allowed distance in cm
  uint8_t persistToNvs;   // 0/1
  uint8_t reserved0;
};
```

Node behavior:
- Applies mmWave max-distance filter in runtime sample processing.
- If `maxDistanceCm == 0`, range limit is disabled.
- If `persistToNvs == 1`, applied value is stored in NVS and reused on next boot.
- If `persistToNvs == 0`, value is runtime-only and default profile/NVS value remains unchanged.

### WifiWsEndpoint (`Type=21`)

```c
struct WifiWsEndpointState {
  Header header;
  uint8_t connected;  // 0/1
  uint8_t ip[4];      // IPv4 bytes, 0.0.0.0 when disconnected
  uint16_t port;      // websocket port (default 81)
  char path[24];      // websocket path (default "/")
  char hostname[32];  // active hostname (for mdns fallback)
};
```

Node behavior:
- Sends this state on `IdentityReq` response flow.
- Sends this state again when WiFi endpoint changes (connect/disconnect/IP change).
- Sends periodic refresh while connected so master/FE can recover endpoint after transient link loss.

### ServoControl (`Type=13`)

```c
struct ServoControlCommand {
  Header header;
  char group[16];   // named logical group, e.g. "camera_pan_tilt"
  uint8_t channel;  // servo channel index in the group
  uint16_t targetDeg10;
  uint16_t transitionMs; // reserved for smooth move policy; currently accepted as metadata
};
```

Node behavior:
- Delegates command to modular actuator manager.
- Returns one `ServoAckState` as `PacketType::STATE` payload.
- If command fails validation (group/channel/range/driver), `ok=0` and `status` explains reason.

### ModuleListReq (`Type=15`)

```c
struct ModuleListReqCommand {
  Header header;
};
```

Node behavior:
- Enumerates available modules from sensing and actuation managers.
- Sends one or more `ModuleInfo` states.
- Ordering is deterministic: all sensor modules first, then actuator modules.

### ModuleInfo (`Type=16`)

```c
struct ModuleInfoState {
  Header header;
  uint8_t index;      // 0-based index in current response sequence
  uint8_t total;      // total count in this response sequence
  uint8_t domain;     // 1=sensor, 2=actuator
  uint8_t reserved0;
  uint32_t featureBits;
  char id[16];        // module id string (e.g. "dht", "mmwave", "camera", "servo")
};
```

Master parsing guidance:
- Group entries by frame sequence context and `total` value.
- Treat unknown `domain` values as forward-compatible and ignore safely.
- `featureBits` may contain multiple bits for one module (for example camera JPEG + stream).

## Runtime Sequence (Practical for Master)

Typical order after node is linked:
1. Node sends `IdentityState` and `FeaturesState`.
2. Node sends `WifiKeyExchangeState` (when secure channel is ready).
3. Node sends sensor samples (`SensorState`, `MmwaveState`) according to interval/module.
4. Master can send `IdentityReq` anytime to re-sync node metadata (including secure key exchange state).
5. Master can send `WifiCredentialsSecure` (preferred) or `WifiCredentials` (compatibility) to request node WiFi connect.
6. Master can send `ServoControl` and receive `ServoAck` runtime state response.
7. Master can send `ModuleListReq` to discover available modules via ESP-NOW.
8. Master sends periodic `HEARTBEAT` to keep link alive.

In powersave mode:
- Every wake cycle, node waits for link until timeout.
- If linked: sends identity/features, then sends boot sample for each active module.
- Before sleep, node waits until no active master task remains and master activity is idle for `NODE_SLEEP_IDLE_THRESHOLD_MS`.
- If not linked: node sleeps again.

## Recommended Parsing Rules on Master

1. Validate `Frame` (`payloadSize <= 200`, consistent length).
2. Check `PacketHeader.version == 1`.
3. Route by `PacketType`.
4. For binary `STATE`:
   - validate magic/version/type/size
   - decode using struct table above
5. Unknown type/unknown feature bit: ignore, do not crash.

## Contract Sources in Code

- `src/app/espnow/protocol.h`
- `src/app/espnow/state_binary.h`
- `src/app/espnow/slave.cpp`
- `src/app/espnow/contract_checks.cpp`

WebSocket API contract is documented separately in [websocket-contract.md](websocket-contract.md).

Indonesian version: [api-contract_id.md](api-contract_id.md)
