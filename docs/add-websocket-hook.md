# Add WebSocket Hook

Quick guide to add a new modular WebSocket hook without hardcoding behavior in gateway transport.

See docs index: [README.md](README.md)

## 1) Create Hook Class

Create files in `src/app/network/hooks/` and implement `IWebsocketHook` from:

- `src/app/network/hooks/websocket_hook.h`

Required methods:

- `type()`
- `begin(WebsocketGateway&)`
- `loop(WebsocketGateway&)`
- `handleRequest(WebsocketGateway&, uint8_t clientId, JsonVariantConst requestData)`
- `onClientDisconnected(WebsocketGateway&, uint8_t clientId)`

## 2) Keep Hook State Inside Hook Module

Store hook-specific runtime state in your hook class, not in `websocket_gateway`.

Examples of valid state placement:

- stream client slots
- request counters or sequence IDs
- feature readiness flags

## 3) Register Hook in Hook Manager

Register your hook in:

- `src/app/network/hooks/websocket_hook_manager.cpp`

Current registration flow:

- create static hook instance(s)
- call `registerHook(...)` inside `ensureRegistryInitialized()`

## 4) Handle Request Envelope Consistently

Incoming request envelope format:

```json
{"type":"<hook-type>","data":{...}}
```

In `handleRequest(...)`, parse `requestData` and return response through `WebsocketGateway` helpers:

- `sendEnvelope(...)` for JSON response
- `sendBinary(...)` for binary payload
- `sendError(...)` for structured error response

## 5) Use SpiJsonDocument for JSON

For JSON parsing/writing, use:

- `SpiJsonDocument`
- include from `lib/SpiJsonDocument/src/SpiJsonDocument.h`

This keeps allocator behavior aligned with the project memory strategy.

## 6) Add Optional Config Macros (If Needed)

If your hook needs tunable defaults, define macros in:

- `include/app_config.h`

Use `#ifndef` style defaults to keep local overrides easy.

## 7) Update WebSocket Contract Docs

If your hook introduces new request states or payload formats, update:

- `docs/websocket-contract.md`
- `docs/websocket-contract_id.md`

## 8) Validate Build

At minimum run a WiFi-enabled build:

```bash
pio run -e dev
```

For camera-related hooks, also validate:

```bash
pio run -e esp32-cam-ai-thinker
```

## 9) Smoke Test Request

Example request over WebSocket:

```json
{"type":"system","data":{"state":"hooks"}}
```

Use this to verify your new hook appears in runtime hook list.

Indonesian version: [add-websocket-hook_id.md](add-websocket-hook_id.md)
