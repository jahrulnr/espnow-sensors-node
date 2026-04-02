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

Build full matrix at once:

```bash
pio run -e esp32-c3-super-mini -e esp32-c3-super-mini-deep -e esp32-c3-super-mini-light -e esp32-s3-devkitc1-n16r8
```

## Required Validation

- Build matrix across profiles must succeed.
- Boot log must show one initial sample per active module.
- In powersave mode:
  - Deep sleep follows default `10s` interval
  - Light sleep follows default `300ms` interval
- Payload contract is guarded by static checks in:
  - `src/app/espnow/contract_checks.cpp`

## WiFi Command Validation (Optional)

Prerequisites:
- Build with `ENABLE_WIFI_MODE=1`.
- Master sends `PacketType::COMMAND` with `Type=WifiCredentials` payload.

Expected:
- Node logs that connect request is queued.
- Node tries to connect to received SSID.
- On success, logs show hostname and mDNS endpoint `<hostname>.local`.
- If no credentials command is sent by master, node does not connect WiFi.

Indonesian version: [build-and-verify_id.md](build-and-verify_id.md)
