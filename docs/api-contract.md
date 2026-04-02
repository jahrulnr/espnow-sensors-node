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
- Node scans channels `1..13` every `300ms`.
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
};
```

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

Commands other than `IdentityReq` and `WifiCredentials` are currently ignored.

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

## Runtime Sequence (Practical for Master)

Typical order after node is linked:
1. Node sends `IdentityState` and `FeaturesState`.
2. Node sends sensor samples (`SensorState`, `MmwaveState`) according to interval/module.
3. Master can send `IdentityReq` anytime to re-sync node metadata.
4. Master sends periodic `HEARTBEAT` to keep link alive.

In powersave mode:
- Every wake cycle, node waits for link until timeout.
- If linked: sends identity/features, then sends boot sample for each active module.
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

Indonesian version: [api-contract_id.md](api-contract_id.md)
