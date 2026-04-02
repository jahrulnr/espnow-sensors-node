---
description: "Use when changing ESP-NOW or WebSocket payloads, request/response contracts, or integration behavior. Keeps contract docs separated and docs navigation consistent."
name: "Contracts And Docs"
---
# Contracts And Docs

- Keep ESP-NOW contract and WebSocket contract in separate docs files.
- When a contract or behavior changes, update related contract docs in `docs/` and keep links in docs index/readme consistent.
- Treat EN/ID docs updates as case-by-case: update both when the change is broadly user-facing, but allow targeted updates when scope is intentionally limited.
- Avoid silent wire-format or envelope changes without corresponding contract documentation updates.
