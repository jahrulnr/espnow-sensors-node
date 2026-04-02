---
description: "Use when adding websocket features, new websocket types, or hook handlers. Enforces modular hook architecture and prevents hardcoded behavior in websocket gateway."
name: "Modular WebSocket Hooks"
---
# Modular WebSocket Hooks

- Keep `websocket_gateway` focused on transport and request routing.
- Put feature-specific behavior in dedicated hook modules under `src/app/network/hooks/`.
- Follow interface + manager registration patterns (similar to sensing modules) instead of adding inline hardcoded branches in gateway/orchestrator files.
- Store runtime state inside each hook module (not in gateway), including stream/session state and feature readiness.
- Prefer reusable helper functions inside hook modules for request parsing and response generation.
