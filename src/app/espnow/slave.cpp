#include "slave.h"

#include "slave_command_hooks.h"
#include "slave_internal.h"
#include "slave_runtime_support.h"

#include "app/network/wifi_manager.h"
#include "app/power/sleep_guard.h"
#include <WiFi.h>
#include <cstring>
#include <esp_log.h>
#include <esp_wifi.h>

namespace app::espnow {

SlaveNode* SlaveNode::activeInstance = nullptr;
SlaveNode espnowSlave;

bool SlaveNode::begin(uint8_t channel, bool enablePipeline) {
  if (started) {
    return true;
  }

  if (!app::espnow::runtime::ensureWifiStaReady(kSlaveLogTag)) {
    return false;
  }

  uint8_t startChannel = app::espnow::runtime::chooseInitialScanChannel(channel, kSlaveLogTag);

  if (startChannel > 0) {
    ESP_LOGI(kSlaveLogTag, "begin(): set initial channel=%u", startChannel);
    esp_err_t channelErr = esp_wifi_set_channel(startChannel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(kSlaveLogTag, "Failed to set WiFi channel %d: %s", startChannel, esp_err_to_name(channelErr));
    }
    scanChannel = startChannel;
  } else {
    scanChannel = kMinScanChannel;
  }

  ESP_LOGI(kSlaveLogTag, "begin(): esp_now_init");
  esp_err_t initErr = esp_now_init();
  if (initErr != ESP_OK) {
    ESP_LOGE(kSlaveLogTag, "esp_now_init failed: %s", esp_err_to_name(initErr));
    return false;
  }

  ESP_LOGI(kSlaveLogTag, "begin(): register callbacks");
  activeInstance = this;
  esp_now_register_send_cb(SlaveNode::onSendStatic);
  esp_now_register_recv_cb(SlaveNode::onReceiveStatic);

  if (!esp_now_is_peer_exist(kBroadcastMac)) {
    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, kBroadcastMac, 6);
    broadcastPeer.ifidx = WIFI_IF_STA;
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    const esp_err_t addBroadcast = esp_now_add_peer(&broadcastPeer);
    if (addBroadcast != ESP_OK && addBroadcast != ESP_ERR_ESPNOW_EXIST) {
      ESP_LOGW(kSlaveLogTag, "Failed adding broadcast peer: %s", esp_err_to_name(addBroadcast));
    }
  }

#if NODE_ESPNOW_SET_PEER_RATE
  {
    esp_now_rate_config_t config = {};
    config.phymode = static_cast<wifi_phy_mode_t>(NODE_ESPNOW_PEER_PHY_MODE);
    config.rate = static_cast<wifi_phy_rate_t>(NODE_ESPNOW_PEER_PHY_RATE);
    config.ersu = NODE_ESPNOW_PEER_RATE_ERSU != 0;
    config.dcm = NODE_ESPNOW_PEER_RATE_DCM != 0;
    const esp_err_t rateErr = esp_now_set_peer_rate_config(kBroadcastMac, &config);
    if (rateErr != ESP_OK && rateErr != ESP_ERR_ESPNOW_NOT_FOUND) {
      ESP_LOGW(kSlaveLogTag, "Failed set broadcast rate: %s", esp_err_to_name(rateErr));
    }
  }
#endif

  started = true;
  lastHelloMs = millis();
  lastScanMs = millis();
  masterCount = 0;
  memset(masters, 0, sizeof(masters));
  (void)enablePipeline;

  ESP_LOGI(kSlaveLogTag, "ESP-NOW slave ready");
  return true;
}

void SlaveNode::loop() {
  if (!started) {
    return;
  }

  app::network::wifiManager.loop();

  const uint32_t now = millis();
  pruneMasters(now);

  // While master is linked, keep channel stable to protect TX/ACK timing.
  // Resume scan only after link is lost so reacquisition still works.
  if (masterCount == 0 &&
      now - lastScanMs >= kChannelScanIntervalMs &&
      !app::network::wifiManager.isChannelLocked()) {
    scanNextChannel();
    lastScanMs = now;
  }

  if (masterCount > 0 && (now - lastHelloMs >= kHelloIntervalMs)) {
    static const char hello[] = "slave-online";
    sendToKnownMasters(PacketType::HELLO, hello, sizeof(hello) - 1);
    lastHelloMs = now;
  }
}

void SlaveNode::onWakeCycleLinkResult(bool linked) {
  app::espnow::runtime::applyWakeResultToCache(linked, kSlaveLogTag);
}

void SlaveNode::markMasterActivity() {
  app::power::touchMasterActivity();
}

bool SlaveNode::sendModuleListSnapshot() {
  if (!started || masterCount == 0) {
    return false;
  }

  return app::espnow::hooks::sendModuleListNow(*this, kSlaveLogTag);
}

void SlaveNode::resetMasterTracking() {
  masterCount = 0;
  memset(masters, 0, sizeof(masters));
}

bool SlaveNode::sendState(const char* text) {
  if (text == nullptr) {
    return false;
  }

  return sendToKnownMasters(PacketType::STATE, text, strlen(text));
}

bool SlaveNode::sendStateBinary(const void* payload, size_t payloadSize) {
  if (payload == nullptr || payloadSize == 0) {
    return false;
  }

  bool sent = sendToKnownMasters(PacketType::STATE, payload, payloadSize);

#if NODE_STATE_BROADCAST_MIRROR
  // Mirror state to broadcast as fallback when unicast ACK is unreliable.
  sent = sendToMaster(kBroadcastMac, scanChannel, PacketType::STATE, payload, payloadSize) || sent;
#endif

  return sent;
}

}  // namespace app::espnow
