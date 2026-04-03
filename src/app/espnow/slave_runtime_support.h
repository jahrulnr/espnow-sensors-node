#pragma once

#include <cstdint>

namespace app::espnow::runtime {

bool ensureWifiStaReady(const char* logTag);

uint8_t chooseInitialScanChannel(uint8_t requestedChannel, const char* logTag);
uint8_t chooseNextScanChannel(uint8_t currentChannel);

void updateMasterCacheEntry(const uint8_t mac[6], uint8_t channel);
void applyWakeResultToCache(bool linked, const char* logTag);

}  // namespace app::espnow::runtime