#pragma once

#include <app_config.h>

#include <cstdint>

namespace app::espnow {

inline constexpr const char* kSlaveLogTag = "espnow_slave";
inline constexpr uint8_t kMinScanChannel = 1;
inline constexpr uint8_t kMaxScanChannel = 13;
inline constexpr uint32_t kChannelScanIntervalMs = NODE_SCAN_CHANNEL_DWELL_MS;
inline constexpr uint32_t kMasterTimeoutMs = 12000;
inline constexpr uint32_t kHelloIntervalMs = 7000;
inline constexpr uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}  // namespace app::espnow