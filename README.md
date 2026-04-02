# ESP-NOW Sensors Node

[![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-orange?logo=platformio)](https://platformio.org/)
![MCU](https://img.shields.io/badge/MCU-ESP32--C3%20%7C%20ESP32--S3-1f6feb)
![Protocol](https://img.shields.io/badge/protocol-v1-2ea44f)
![WiFi](https://img.shields.io/badge/WiFi-command--driven-blue)
[![Docs](https://img.shields.io/badge/docs-wiki-0ea5e9)](docs/README.md)

A modular ESP32 field node for telemetry and device control across boards and profiles.

## Who This Is For

This project is designed for:

- **Smart home builders** — deploy sensor + actuator nodes around the house, controlled from a central master over ESP-NOW without relying on cloud services.
- **AI agent IoT systems** — the slave exposes its identity and module inventory via ESP-NOW protocol, so an AI agent on the master side can discover capabilities dynamically and act on them (e.g. "turn servo to 90°", "is there motion in room 2?").
- **Embedded hobbyists and makers** — swap board targets, swap sensor/actuator modules via profile config, no firmware rewrite needed.
- **Multi-node environments** — run multiple slaves with different profiles, all reporting to one master using the same binary protocol contract.

## What This Project Is For

Main goals of this project:
- Make the node flexible across board targets.
- Allow sensing and control module combinations to be switched via profile configuration.
- Keep `boot`, `input`, and `network` pipelines separated and maintainable.
- Provide a stable binary contract so master-side integrations can discover and interact with any slave node without hardcoding capabilities.

Example deployments:
- ESP32-C3: `mmwave + dht` → presence + climate sensor node
- ESP32-S3: `dht` → lightweight climate node with deep sleep
- ESP32-CAM (AI Thinker): `camera + ESP-NOW-first control` → vision node with pan/tilt servo
- Any board: add a profile, add modules, flash and go.

## Start Here

If you want to:
- See the documentation map: [docs/README.md](docs/README.md)
- Understand the modular architecture: [docs/architecture.md](docs/architecture.md)
- Configure board profiles, pins, and powersave: [docs/configuration.md](docs/configuration.md)
- Build and verify: [docs/build-and-verify.md](docs/build-and-verify.md)
- Integrate a master using the protocol contract: [docs/api-contract.md](docs/api-contract.md)
- Integrate clients over WebSocket gateway: [docs/websocket-contract.md](docs/websocket-contract.md)
- Add custom WebSocket hooks: [docs/add-websocket-hook.md](docs/add-websocket-hook.md)
- Add a new sensor type: [docs/add-sensor-module.md](docs/add-sensor-module.md)
- Discover available slave modules via ESP-NOW: [docs/api-contract.md](docs/api-contract.md)

## Notes

- Technical details are centralized in the [docs](docs/) folder.
- Indonesian version: [README_id.md](README_id.md)
