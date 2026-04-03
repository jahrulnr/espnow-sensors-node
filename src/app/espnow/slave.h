#pragma once

#include <Arduino.h>
#include <esp_now.h>

#include "protocol.h"

namespace app::espnow {

class SlaveNode {
 public:
  SlaveNode() = default;

  bool begin(uint8_t channel = 1, bool enablePipeline = true);
  void loop();

  bool sendState(const char* text);
  bool sendStateBinary(const void* payload, size_t payloadSize);
  bool sendModuleListSnapshot();
  void resetMasterTracking();
  bool isReady() const { return started; }
  bool isMasterLinked() const { return masterCount > 0; }
  void onWakeCycleLinkResult(bool linked);
  void markMasterActivity();

 private:
  static constexpr size_t MAX_TRACKED_MASTERS = 8;

  struct MasterPeer {
    bool used = false;
    uint8_t mac[6] = {0};
    uint8_t channel = DEFAULT_CHANNEL;
    uint32_t lastSeenMs = 0;
  };

  static void onSendStatic(const esp_now_send_info_t* tx_info, esp_now_send_status_t status);
  static void onReceiveStatic(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);

  int findMasterIndex(const uint8_t mac[6]) const;
  void scanNextChannel();
  void pruneMasters(uint32_t nowMs);
  bool addMasterPeer(const uint8_t mac[6], uint8_t channel, uint32_t seenMs);
  bool sendToMaster(const uint8_t mac[6], uint8_t channel, PacketType type, const void* payload, size_t payloadSize);
  bool sendToKnownMasters(PacketType type, const void* payload, size_t payloadSize);

  static SlaveNode* activeInstance;

  uint16_t sequence = 0;
  bool started = false;
  size_t masterCount = 0;
  MasterPeer masters[MAX_TRACKED_MASTERS] = {};
  uint8_t scanChannel = DEFAULT_CHANNEL;

  uint32_t lastHelloMs = 0;
  uint32_t lastScanMs = 0;
};

extern SlaveNode espnowSlave;

}  // namespace app::espnow
