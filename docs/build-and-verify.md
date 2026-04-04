# Build and Verify

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

Build C3 with WiFi mode enabled:

```bash
pio run -e esp32-c3-super-mini-wifi
```

Build AI Thinker ESP32-CAM profile:

```bash
pio run -e esp32-cam-ai-thinker
```

Build full matrix at once:

```bash
pio run -e esp32-c3-super-mini -e esp32-c3-super-mini-deep -e esp32-c3-super-mini-light -e esp32-s3-devkitc1-n16r8
```

## Required Validation

- Build matrix across profiles must succeed.
- Boot log must show one initial sample per active module.
- Boot log should not report unexpected actuator initialization failures when profiles disable outputs.
- In powersave mode:
  - Deep sleep follows default `10s` interval
  - Light sleep follows default `300ms` interval
- Payload contract is guarded by static checks in:
  - `src/app/espnow/contract_checks.cpp`

## ESP-NOW Control + Discovery Validation (Recommended)

Prerequisites:
- Master has linked to node (`HELLO` / `HEARTBEAT` flow healthy).

Validate discovery:
- Send `PacketType::COMMAND` with `Type=IdentityReq` and confirm node replies with `IdentityState` and `FeaturesState`.
- Send `PacketType::COMMAND` with `Type=ModuleListReq` and confirm node replies with one or more `ModuleInfo` states.
- Confirm `ModuleInfo.index` increments from `0` and `ModuleInfo.total` remains consistent for one sequence.

Validate control (when servo output is enabled in profile):
- Send `PacketType::COMMAND` with `Type=ServoControl`.
- Confirm node replies with `ServoAck` state.
- Verify error behavior for invalid group/channel/range returns `ok=0` and proper `status` code.

## WiFi Command Validation (Optional)

Prerequisites:
- Build with `ENABLE_WIFI_MODE=1`.
- Master sends `PacketType::COMMAND` with `Type=WifiCredentials` payload.

Expected:
- Node logs that connect request is queued.
- Node tries to connect to received SSID.
- On success, logs show hostname and mDNS endpoint `<hostname>.local`.
- If no credentials command is sent by master, node does not connect WiFi.

## Secure WiFi Provisioning Validation (Optional)

Prerequisites:
- Build with `ENABLE_WIFI_MODE=1`.
- Master supports `WifiKeyExchange` (`Type=18`) and `WifiCredentialsSecure` (`Type=19`).

Expected discovery behavior:
- After master sends `IdentityReq`, node sends `IdentityState`, `FeaturesState`, and `WifiKeyExchange`.
- `WifiKeyExchange.publicKeySize` should be `33` and `curve` should be `1` (secp256r1).

Expected secure command behavior:
- When receiving valid `Type=19`, node decrypts credentials and queues WiFi connect request.
- When replaying the same `Type=19` payload (`counter` unchanged), node rejects command (anti-replay) and does not queue connect.
- When `keyId` does not match local key state, node rejects command safely.

## mmWave Range Override Validation (Optional)

Prerequisites:
- mmWave module is enabled and streaming valid reports.
- Master can send `PacketType::COMMAND` with `Type=MmwaveRangeConfig (20)`.

Expected:
- Command payload with `maxDistanceCm` and `persistToNvs` is accepted by node command hook.
- If `maxDistanceCm` is smaller than live target distance, node output should clamp to no-detection (`targetState=0`, `detected=0`).
- If `persistToNvs=1`, configured range should remain active after reboot.
- If `persistToNvs=0`, runtime override should not overwrite persisted/default setting.

## WebSocket Validation (Optional)

Prerequisites:
- WiFi command flow already connected the node.

Expected:
- Log indicates websocket gateway active on configured port.
- Sending JSON request with envelope `{"type":"system","data":{"state":"hooks"}}` returns JSON envelope response.
- Sending `{"type":"camera","data":{"state":"specs"}}` returns camera specs JSON.

Indonesian version: [build-and-verify_id.md](build-and-verify_id.md)
