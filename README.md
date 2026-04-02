# ESP-NOW Sensors Node

[![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-orange?logo=platformio)](https://platformio.org/)
![MCU](https://img.shields.io/badge/MCU-ESP32--C3%20%7C%20ESP32--S3-1f6feb)
![Protocol](https://img.shields.io/badge/protocol-v1-2ea44f)
![WiFi](https://img.shields.io/badge/WiFi-command--driven-blue)
[![Docs](https://img.shields.io/badge/docs-wiki-0ea5e9)](docs/README.md)

A modular ESP32-based sensor node for multi-purpose sensor collector scenarios across boards and profiles.

## What This Project Is For

Main goals of this project:
- Make the sensor node flexible across board targets.
- Allow sensor combinations to be switched via profile configuration.
- Keep `boot`, `input`, and `network` pipelines separated and maintainable.

Example scenarios:
- ESP32-C3: `mmwave + dht`
- ESP32-S3: `dht`
- ESP32-CAM (AI Thinker): `camera-ready profile + WiFi mode`
- Other boards: just add a board profile and sensor modules.

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

## Notes

- Technical details are centralized in the [docs](docs/) folder.
- Indonesian version: [README_id.md](README_id.md)
