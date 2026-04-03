#include "slave.h"

#include "slave_command_hooks.h"
#include "slave_internal.h"
#include "slave_runtime_support.h"
#include "state_binary.h"

#include "app/network/wifi_manager.h"

#include <esp_log.h>
#include <esp_wifi.h>

namespace app::espnow {

int SlaveNode::findMasterIndex(const uint8_t mac[6]) const {
  if (mac == nullptr) {
    return -1;
  }

  for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
    if (masters[i].used && memcmp(masters[i].mac, mac, 6) == 0) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void SlaveNode::scanNextChannel() {
  if (app::network::wifiManager.isChannelLocked()) {
    return;
  }

  const uint8_t nextChannel = app::espnow::runtime::chooseNextScanChannel(scanChannel);

  const esp_err_t err = esp_wifi_set_channel(nextChannel, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    scanChannel = nextChannel;
    ESP_LOGD(kSlaveLogTag, "Scanning channel %u", scanChannel);
    return;
  }

  ESP_LOGW(kSlaveLogTag, "Failed switching to channel %u: %s", nextChannel, esp_err_to_name(err));
}

void SlaveNode::pruneMasters(uint32_t nowMs) {
  for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
    if (!masters[i].used) {
      continue;
    }

    if (nowMs - masters[i].lastSeenMs <= kMasterTimeoutMs) {
      continue;
    }

    ESP_LOGW(kSlaveLogTag,
             "Master timeout: %02X:%02X:%02X:%02X:%02X:%02X",
             masters[i].mac[0],
             masters[i].mac[1],
             masters[i].mac[2],
             masters[i].mac[3],
             masters[i].mac[4],
             masters[i].mac[5]);
    masters[i].used = false;
    masters[i].lastSeenMs = 0;
    masters[i].channel = DEFAULT_CHANNEL;
    memset(masters[i].mac, 0, sizeof(masters[i].mac));
    if (masterCount > 0) {
      masterCount--;
    }
  }
}

bool SlaveNode::addMasterPeer(const uint8_t mac[6], uint8_t channel, uint32_t seenMs) {
  if (mac == nullptr) {
    return false;
  }

  if (channel < kMinScanChannel || channel > kMaxScanChannel) {
    channel = DEFAULT_CHANNEL;
  }

  const int existingIndex = findMasterIndex(mac);
  if (existingIndex >= 0) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    const esp_err_t modErr = esp_now_mod_peer(&peer);
    if (modErr != ESP_OK && modErr != ESP_ERR_ESPNOW_NOT_FOUND) {
      ESP_LOGW(kSlaveLogTag, "Failed to sync master peer channel: %s", esp_err_to_name(modErr));
    }

    masters[existingIndex].channel = channel;
    masters[existingIndex].lastSeenMs = seenMs;
    app::espnow::runtime::updateMasterCacheEntry(mac, channel);
    return true;
  }

  if (esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    const esp_err_t modErr = esp_now_mod_peer(&peer);
    if (modErr != ESP_OK && modErr != ESP_ERR_ESPNOW_NOT_FOUND) {
      ESP_LOGW(kSlaveLogTag, "Failed to sync existing peer channel: %s", esp_err_to_name(modErr));
    }

    for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
      if (masters[i].used) {
        continue;
      }

      masters[i].used = true;
      memcpy(masters[i].mac, mac, 6);
      masters[i].channel = channel;
      masters[i].lastSeenMs = seenMs;
      masterCount++;
      app::espnow::runtime::updateMasterCacheEntry(mac, channel);
      ESP_LOGI(kSlaveLogTag,
               "Master tracked: %02X:%02X:%02X:%02X:%02X:%02X on ch %u",
               mac[0],
               mac[1],
               mac[2],
               mac[3],
               mac[4],
               mac[5],
               channel);
      return true;
    }

    ESP_LOGW(kSlaveLogTag, "Master list full, cannot track existing peer");
    return false;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = 0;
  peer.encrypt = false;

  esp_err_t addErr = esp_now_add_peer(&peer);
  if (addErr != ESP_OK) {
    ESP_LOGW(kSlaveLogTag, "Failed add master peer: %s", esp_err_to_name(addErr));
    return false;
  }

  for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
    if (masters[i].used) {
      continue;
    }

    masters[i].used = true;
    memcpy(masters[i].mac, mac, 6);
    masters[i].channel = channel;
    masters[i].lastSeenMs = seenMs;
    masterCount++;
    app::espnow::runtime::updateMasterCacheEntry(mac, channel);
    ESP_LOGI(kSlaveLogTag,
             "Master registered: %02X:%02X:%02X:%02X:%02X:%02X on ch %u",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5],
             channel);
    return true;
  }

  ESP_LOGW(kSlaveLogTag, "Master list full, cannot track new peer");
  return false;
}

bool SlaveNode::sendToMaster(const uint8_t mac[6],
                             uint8_t channel,
                             PacketType type,
                             const void* payload,
                             size_t payloadSize) {
  if (!started || mac == nullptr) {
    return false;
  }

  if (!app::network::wifiManager.isChannelLocked() && channel >= kMinScanChannel && channel <= kMaxScanChannel) {
    const esp_err_t channelErr = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(kSlaveLogTag, "Failed to switch channel %u before send: %s", channel, esp_err_to_name(channelErr));
      return false;
    }
    scanChannel = channel;
  }

  Frame frame = {};
  frame.header.version = PROTOCOL_VERSION;
  frame.header.type = static_cast<uint8_t>(type);
  frame.header.sequence = sequence++;
  frame.header.timestampMs = millis();

  frame.payloadSize = payloadSize > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : payloadSize;
  if (frame.payloadSize > 0 && payload != nullptr) {
    memcpy(frame.payload, payload, frame.payloadSize);
  }

  const size_t bytes = sizeof(frame.header) + sizeof(frame.payloadSize) + frame.payloadSize;

  // Keep peer channel policy dynamic to avoid stale fixed-channel entries across reconnect cycles.
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = 0;
  peer.encrypt = false;
  const esp_err_t modErr = esp_now_mod_peer(&peer);
  if (modErr == ESP_ERR_ESPNOW_NOT_FOUND) {
    const esp_err_t addErr = esp_now_add_peer(&peer);
    if (addErr != ESP_OK && addErr != ESP_ERR_ESPNOW_EXIST) {
      ESP_LOGW(kSlaveLogTag, "Failed to add peer before send: %s", esp_err_to_name(addErr));
      return false;
    }
  } else if (modErr != ESP_OK) {
    ESP_LOGW(kSlaveLogTag, "Failed to update peer before send: %s", esp_err_to_name(modErr));
  }

  esp_err_t sendErr = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&frame), bytes);
  if (sendErr != ESP_OK) {
    ESP_LOGW(kSlaveLogTag, "Send to master failed: %s", esp_err_to_name(sendErr));
    return false;
  }

  return true;
}

bool SlaveNode::sendToKnownMasters(PacketType type, const void* payload, size_t payloadSize) {
  if (!started || masterCount == 0) {
    return false;
  }

  bool sentAny = false;
  for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
    if (!masters[i].used) {
      continue;
    }

    sentAny = sendToMaster(masters[i].mac, masters[i].channel, type, payload, payloadSize) || sentAny;
  }

  return sentAny;
}

void SlaveNode::onSendStatic(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
  if (!activeInstance) {
    return;
  }

  if (status != ESP_NOW_SEND_SUCCESS) {
    uint8_t currentPrimary = 0;
    wifi_second_chan_t currentSecondary = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&currentPrimary, &currentSecondary) != ESP_OK) {
      currentPrimary = 0;
    }

    if (tx_info != nullptr && tx_info->des_addr != nullptr) {
      ESP_LOGW(kSlaveLogTag,
               "TX failed -> %02X:%02X:%02X:%02X:%02X:%02X (ch=%u)",
               tx_info->des_addr[0],
               tx_info->des_addr[1],
               tx_info->des_addr[2],
               tx_info->des_addr[3],
               tx_info->des_addr[4],
               tx_info->des_addr[5],
               static_cast<unsigned>(currentPrimary));
    } else {
      ESP_LOGW(kSlaveLogTag, "TX failed (no tx_info, ch=%u)", static_cast<unsigned>(currentPrimary));
    }
  }

  if (tx_info == nullptr) {
    ESP_LOGD(kSlaveLogTag, "TX done -> %s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
    return;
  }

  ESP_LOGD(kSlaveLogTag, "TX status=%s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

void SlaveNode::onReceiveStatic(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
  if (!activeInstance || recv_info == nullptr || data == nullptr || len <= 0) {
    return;
  }

  if (len < static_cast<int>(sizeof(PacketHeader) + sizeof(uint8_t))) {
    ESP_LOGW(kSlaveLogTag, "Received frame too small: %d", len);
    return;
  }

  const auto* header = reinterpret_cast<const PacketHeader*>(data);
  const auto payloadSize = *(data + sizeof(PacketHeader));
  const auto* payload = data + sizeof(PacketHeader) + sizeof(uint8_t);
  const size_t expectedLen = sizeof(PacketHeader) + sizeof(uint8_t) + payloadSize;
  if (payloadSize > MAX_PAYLOAD_SIZE || expectedLen > static_cast<size_t>(len)) {
    ESP_LOGW(kSlaveLogTag, "Invalid frame size: payload=%u len=%d", payloadSize, len);
    return;
  }

  const auto type = static_cast<PacketType>(header->type);
  const int knownIndex = activeInstance->findMasterIndex(recv_info->src_addr);
  const bool fromKnownMaster = knownIndex >= 0;

  uint8_t currentChannel = DEFAULT_CHANNEL;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&currentChannel, &second) != ESP_OK) {
    currentChannel = DEFAULT_CHANNEL;
  }

  if (!fromKnownMaster) {
    if (type != PacketType::HELLO && type != PacketType::HEARTBEAT) {
      ESP_LOGD(kSlaveLogTag, "Ignoring packet from unknown sender");
      return;
    }

    if (!activeInstance->addMasterPeer(recv_info->src_addr, currentChannel, millis())) {
      return;
    }
  } else {
    activeInstance->masters[knownIndex].lastSeenMs = millis();
    activeInstance->masters[knownIndex].channel = currentChannel;
  }

  if (type == PacketType::COMMAND) {
    activeInstance->markMasterActivity();
  }

  if (type == PacketType::HELLO || type == PacketType::HEARTBEAT) {
    activeInstance->scanChannel = currentChannel;
  }

  switch (type) {
    case PacketType::HELLO: {
      static const char hello[] = "slave-online";
      activeInstance->sendToMaster(recv_info->src_addr, currentChannel, PacketType::HELLO, hello, sizeof(hello) - 1);
      break;
    }
    case PacketType::HEARTBEAT: {
      app::espnow::state_binary::SlaveAliveState state = {};
      app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::SlaveAlive);
      activeInstance->sendToMaster(recv_info->src_addr, currentChannel, PacketType::STATE, &state, sizeof(state));
      break;
    }
    case PacketType::COMMAND:
      (void)app::espnow::hooks::handleCommandPacket(*activeInstance, payload, payloadSize, kSlaveLogTag);
      break;
    case PacketType::STATE:
      if (payloadSize > 0) {
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::MasterNet,
                                                      sizeof(app::espnow::state_binary::MasterNetState))) {
          const auto* state = reinterpret_cast<const app::espnow::state_binary::MasterNetState*>(payload);
          ESP_LOGI("MASTER", "Internet=%s channel=%u", state->online == 1 ? "UP" : "DOWN", state->channel);
        }
      }
    default:
      break;
  }
}

}  // namespace app::espnow