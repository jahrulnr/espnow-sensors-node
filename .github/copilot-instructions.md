# AI Agent Instructions (Repository-Specific)

This file is the fast-start map for AI agents working in this repo.

## 1) Project Reality (Current Architecture)

- Platform: PlatformIO + Arduino (ESP32 family).
- Main target today: ESP-NOW sensor node with modular sensing pipeline.
- Runtime entry: `src/main.cpp`.
- Core flow:
  1. `app::boot::run()` initializes sensor modules and logs one boot sample per active module.
  2. `networkTask` manages ESP-NOW link and transport.
  3. `inputTask` (when powersave is disabled) polls sensors and publishes encoded binary payloads.

This repo is sensor-modular and protocol-contract driven.

## 2) Read Order Before Editing

Read these first:
1. `docs/README.md`
2. `docs/architecture.md`
3. `docs/configuration.md`
4. `docs/api-contract.md`
5. `docs/websocket-contract.md`
6. `docs/build-and-verify.md`
7. `docs/add-websocket-hook.md` (when adding websocket hooks/types)
8. `docs/add-sensor-module.md` (only when adding sensor types)

## 3) Source Index (Where To Edit What)

### Protocol and wire contract
- `src/app/espnow/protocol.h`
- `src/app/espnow/state_binary.h`
- `src/app/espnow/contract_checks.cpp` (static asserts; keep in sync with protocol updates)
- `src/app/espnow/slave.cpp` (receive/send behavior)

### Sensor modular layer
- `src/app/sensing/sensor_module.h` (`ISensorModule`)
- `src/app/sensing/sensor_sample.h`
- `src/app/sensing/sensor_manager.h/.cpp`
- `src/app/sensing/sensor_encoder.h/.cpp`
- `src/app/sensing/modules/*.h/.cpp` (module adapters)

### Runtime boundaries
- Boot hook: `src/app/boot/boot.cpp`
- Input pipeline: `src/app/tasks/inputTask.cpp`
- Network pipeline: `src/app/tasks/networkTask.cpp`
- WiFi command-driven client: `src/app/network/wifi_manager.h/.cpp`
- WebSocket gateway transport/router: `src/app/network/websocket_gateway.h/.cpp`
- WebSocket modular hooks: `src/app/network/hooks/*.h/.cpp`

### Config and profile selection
- Global policy: `include/app_config.h`
- Profile selector: `include/board_profile.h`
- Board profiles: `include/profiles/profile_*.h`
- Local user override profile: `include/profiles/profile_user.h` (gitignored)
- Pin mapping bridge: `include/hw.h`
- Build env matrix: `platformio.ini`

## 4) Non-Negotiable Boundaries

- `networkTask` must not read sensor drivers directly.
- Sensor driver usage belongs in sensing modules only.
- `websocket_gateway` must remain transport/router only; feature logic belongs in hook modules.
- Keep ESP-NOW and WebSocket contracts in separate docs/files.
- Payload format changes must update:
  - `state_binary.h`
  - `contract_checks.cpp`
  - `docs/api-contract.md`
- New sensor type should not require `networkTask` code changes.
- Keep boot sampling in `app::boot`, not in task logic.

## 5) WiFi Feature Rules (Current Contract)

- WiFi connection is command-driven from master.
- Node connects only if:
  - `ENABLE_WIFI_MODE == 1`, and
  - master sends `COMMAND` with `Type::WifiCredentials`.
- If no credentials command arrives: skip WiFi connect.
- Hostname is local config (`WIFI_CLIENT_HOSTNAME`) and exposed via mDNS as `<hostname>.local`.
- WebSocket gateway starts only after WiFi is connected.

If you touch WiFi behavior, verify docs and contract remain accurate.

## 6) Build/Validation Checklist For Agents

Minimum after changes:

```bash
pio run -e esp32-c3-super-mini
```

For modular/protocol/core changes run matrix:

```bash
pio run -e esp32-c3-super-mini -e esp32-c3-super-mini-deep -e esp32-c3-super-mini-light -e esp32-s3-devkitc1-n16r8
```

For WiFi codepath validation:

```bash
pio run -e esp32-c3-super-mini-wifi
```

For ESP32-CAM + websocket path validation:

```bash
pio run -e esp32-cam-ai-thinker
```

No unit-test suite is provided; build success + contract consistency is mandatory.

## 7) Best Practices For AI Code Changes

- Prefer minimal, bounded edits in the correct layer.
- Keep feature toggles in `app_config.h` (`#ifndef` + default value).
- Maintain backward-compatible wire behavior unless explicitly requested.
- Add/adjust docs for any behavior or contract change.
- Use ESP logging (`ESP_LOGI/W/E/D`) instead of `Serial.print`.
- Preserve board profile modularity; avoid hardcoding per-board behavior in runtime logic.

## 8) Quick Task Routing (Decision Map)

- "Add sensor type": sensing module + sample shape + encoder + manager registration + docs.
- "Change payload schema": state binary + contract checks + parser behavior + docs/api-contract.
- "Add websocket hook/type": create hook module + manager registration + websocket contract docs.
- "Board-specific pins/features": profile headers + config wiring, not task-level conditionals.
- "User-local pin customization": use `profile_user.h` override, do not edit tracked profile files.
- "Boot behavior": `app::boot` hook first, then verify downstream task assumptions.
- "Master interoperability issue": start from `docs/api-contract.md` and `src/app/espnow/*`.

## 9) Things To Avoid

- Re-introducing legacy weather/cache flow assumptions.
- Mixing protocol-definition and business logic in unrelated files.
- Silent wire-format changes without contract/doc updates.
- Large cross-cutting refactors without preserving modular boundaries.
