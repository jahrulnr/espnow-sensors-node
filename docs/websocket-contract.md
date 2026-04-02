# WebSocket Contract (Node Gateway)

This document defines the multipurpose WebSocket contract for clients that interact with the node over WiFi.

## Availability

WebSocket gateway is available only when:
- `ENABLE_WIFI_MODE == 1`
- Node has connected to WiFi after receiving `WifiCredentials` command from master
- `ENABLE_WEBSOCKET_GATEWAY == 1`

Default endpoint:
- `ws://<node-ip>:81/`

Port can be changed with `WEBSOCKET_SERVER_PORT`.

## Envelope Format

All JSON messages use this envelope:

```json
{"type":"<hook-type>","data":{...}}
```

Examples:

Request:

```json
{"type":"camera","data":{"state":"specs"}}
```

Response:

```json
{"type":"camera","data":{"state":"specs","width":320,"height":240,"format":"jpg"}}
```

## Routing Model

- Requests are routed by `type` to registered hooks.
- Unknown `type` returns structured error response.
- JSON parsing and writing uses `SpiJsonDocument`.

## Built-in Hooks

### `system`

Supported request states:
- `{"type":"system","data":{"state":"ping"}}` -> pong envelope
- `{"type":"system","data":{"state":"hooks"}}` -> lists active hook types

### `camera`

Supported request states:
- `{"type":"camera","data":{"state":"specs"}}` -> returns camera capability envelope
- `{"type":"camera","data":{"state":"frame"}}` -> captures one frame and sends JSON metadata + binary JPEG payload
- `{"type":"camera","data":{"state":"stream","action":"start","intervalMs":500}}` -> starts periodic frame streaming for this client
- `{"type":"camera","data":{"state":"stream","action":"stop"}}` -> stops periodic frame streaming for this client

## Error Response Format

Error responses follow this shape:

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

## Binary Response

Response may be JSON or binary depending on hook implementation.

Recommended approach for binary:
- Send a JSON metadata envelope first (type, format, dimensions, sequence).
- Send binary frame payload in subsequent WebSocket binary message(s).

## Contract Sources in Code

- `src/app/network/websocket_gateway.h`
- `src/app/network/websocket_gateway.cpp`
- `src/app/network/wifi_manager.cpp`

Hook implementation guide: [add-websocket-hook.md](add-websocket-hook.md)

Indonesian version: [websocket-contract_id.md](websocket-contract_id.md)
