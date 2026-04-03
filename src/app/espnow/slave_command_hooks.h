#pragma once

#include <cstddef>
#include <cstdint>

namespace app::espnow {
class SlaveNode;
}

namespace app::espnow::hooks {

bool sendIdentityStateNow(SlaveNode& node, const char* logTag);
bool sendFeaturesStateNow(SlaveNode& node, const char* logTag);
bool sendModuleListNow(SlaveNode& node, const char* logTag);

bool handleCommandPacket(SlaveNode& node,
                         const uint8_t* payload,
                         size_t payloadSize,
                         const char* logTag);

}  // namespace app::espnow::hooks
