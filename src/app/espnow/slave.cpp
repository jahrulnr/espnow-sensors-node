#include "slave.h"

#include "state_binary.h"

#include "app/network/wifi_manager.h"
#include "app/sensing/sensor_manager.h"
#include <app_config.h>
#include <WiFi.h>
#include <cstring>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

namespace app::espnow {

namespace {

bool sendIdentityStateNow(SlaveNode& node) {
#if !NODE_SEND_IDENTITY_STATE
  (void)node;
  return true;
#else
  app::espnow::state_binary::IdentityState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
  strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
  state.id[sizeof(state.id) - 1] = '\0';

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("espnow_slave", "Failed sending identity state");
  }
  return sent;
#endif
}

bool sendFeaturesStateNow(SlaveNode& node) {
  app::espnow::state_binary::FeaturesState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
  state.contractVersion = 1;
  state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
                   | app::sensing::sensorManager.featureBits();
#if ENABLE_WIFI_MODE
  state.featureBits |= static_cast<uint32_t>(app::espnow::state_binary::FeatureWifiSta);
#endif

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("espnow_slave", "Failed sending feature state");
  }
  return sent;
}

}  // namespace

static const char* TAG = "espnow_slave";
static constexpr uint8_t MIN_SCAN_CHANNEL = 1;
static constexpr uint8_t MAX_SCAN_CHANNEL = 13;
static constexpr uint32_t CHANNEL_SCAN_INTERVAL_MS = 300;
static constexpr uint32_t MASTER_TIMEOUT_MS = 12000;
static constexpr uint32_t HELLO_INTERVAL_MS = 7000;

bool ensureWifiStaReady() {
  ESP_LOGI(TAG, "begin(): init WiFi STA core");

  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
    return false;
  }

  static bool netifCreated = false;
  if (!netifCreated) {
    esp_netif_create_default_wifi_sta();
    netifCreated = true;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_storage failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_start();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
    ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_wifi_disconnect();
  return true;
}

SlaveNode* SlaveNode::activeInstance = nullptr;
SlaveNode espnowSlave;

bool SlaveNode::begin(uint8_t channel, bool enablePipeline) {
  if (started) {
    return true;
  }

  if (!ensureWifiStaReady()) {
    return false;
  }

  if (channel > 0) {
    ESP_LOGI(TAG, "begin(): set initial channel=%u", channel);
    esp_err_t channelErr = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set WiFi channel %d: %s", channel, esp_err_to_name(channelErr));
    }
    scanChannel = channel;
  } else {
    scanChannel = MIN_SCAN_CHANNEL;
  }

  ESP_LOGI(TAG, "begin(): esp_now_init");
  esp_err_t initErr = esp_now_init();
  if (initErr != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(initErr));
    return false;
  }

  ESP_LOGI(TAG, "begin(): register callbacks");
  activeInstance = this;
  esp_now_register_send_cb(SlaveNode::onSendStatic);
  esp_now_register_recv_cb(SlaveNode::onReceiveStatic);

  started = true;
  lastHelloMs = millis();
  lastScanMs = millis();
  masterCount = 0;
  memset(masters, 0, sizeof(masters));
  (void)enablePipeline;
  ESP_LOGI(TAG, "ESP-NOW slave ready");
  return true;
}

void SlaveNode::loop() {
  if (!started) {
    return;
  }

  app::network::wifiManager.loop();

  const uint32_t now = millis();
  pruneMasters(now);

  // Keep scanning continuously so the slave can discover masters on other channels.
  if (now - lastScanMs >= CHANNEL_SCAN_INTERVAL_MS && !app::network::wifiManager.isChannelLocked()) {
    scanNextChannel();
    lastScanMs = now;
  }

  if (masterCount > 0 && (now - lastHelloMs >= HELLO_INTERVAL_MS)) {
    static const char hello[] = "slave-online";
    sendToKnownMasters(PacketType::HELLO, hello, sizeof(hello) - 1);
    lastHelloMs = now;
  }
}

void SlaveNode::scanNextChannel() {
  if (app::network::wifiManager.isChannelLocked()) {
    return;
  }

  uint8_t nextChannel = scanChannel;
  if (nextChannel < MIN_SCAN_CHANNEL || nextChannel >= MAX_SCAN_CHANNEL) {
    nextChannel = MIN_SCAN_CHANNEL;
  } else {
    nextChannel++;
  }

  const esp_err_t err = esp_wifi_set_channel(nextChannel, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    scanChannel = nextChannel;
    ESP_LOGD(TAG, "Scanning channel %u", scanChannel);
    return;
  }

  ESP_LOGW(TAG, "Failed switching to channel %u: %s", nextChannel, esp_err_to_name(err));
}

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

void SlaveNode::pruneMasters(uint32_t nowMs) {
  for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
    if (!masters[i].used) {
      continue;
    }

    if (nowMs - masters[i].lastSeenMs <= MASTER_TIMEOUT_MS) {
      continue;
    }

    ESP_LOGW(TAG,
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

  if (channel < MIN_SCAN_CHANNEL || channel > MAX_SCAN_CHANNEL) {
    channel = DEFAULT_CHANNEL;
  }

  const int existingIndex = findMasterIndex(mac);
  if (existingIndex >= 0) {
    masters[existingIndex].channel = channel;
    masters[existingIndex].lastSeenMs = seenMs;
    return true;
  }

  if (esp_now_is_peer_exist(mac)) {
    for (size_t i = 0; i < MAX_TRACKED_MASTERS; ++i) {
      if (masters[i].used) {
        continue;
      }

      masters[i].used = true;
      memcpy(masters[i].mac, mac, 6);
      masters[i].channel = channel;
      masters[i].lastSeenMs = seenMs;
      masterCount++;
      ESP_LOGI(TAG,
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

    ESP_LOGW(TAG, "Master list full, cannot track existing peer");
    return false;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = 0;
  peer.encrypt = false;

  esp_err_t addErr = esp_now_add_peer(&peer);
  if (addErr != ESP_OK) {
    ESP_LOGW(TAG, "Failed add master peer: %s", esp_err_to_name(addErr));
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
    ESP_LOGI(TAG,
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

  ESP_LOGW(TAG, "Master list full, cannot track new peer");
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

  if (!app::network::wifiManager.isChannelLocked() && channel >= MIN_SCAN_CHANNEL && channel <= MAX_SCAN_CHANNEL) {
    const esp_err_t channelErr = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(TAG, "Failed to switch channel %u before send: %s", channel, esp_err_to_name(channelErr));
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
  esp_err_t sendErr = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&frame), bytes);
  if (sendErr != ESP_OK) {
    ESP_LOGW(TAG, "Send to master failed: %s", esp_err_to_name(sendErr));
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

  return sendToKnownMasters(PacketType::STATE, payload, payloadSize);
}

void SlaveNode::onSendStatic(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
  if (!activeInstance) {
    return;
  }

  if (tx_info == nullptr) {
    ESP_LOGD(TAG, "TX done -> %s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
    return;
  }

  ESP_LOGD(TAG, "TX status=%s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

void SlaveNode::onReceiveStatic(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
  if (!activeInstance || recv_info == nullptr || data == nullptr || len <= 0) {
    return;
  }

  if (len < static_cast<int>(sizeof(PacketHeader) + sizeof(uint8_t))) {
    ESP_LOGW(TAG, "Received frame too small: %d", len);
    return;
  }

  const auto* header = reinterpret_cast<const PacketHeader*>(data);
  const auto payloadSize = *(data + sizeof(PacketHeader));
  const auto* payload = data + sizeof(PacketHeader) + sizeof(uint8_t);
  const size_t expectedLen = sizeof(PacketHeader) + sizeof(uint8_t) + payloadSize;
  if (payloadSize > MAX_PAYLOAD_SIZE || expectedLen > static_cast<size_t>(len)) {
    ESP_LOGW(TAG, "Invalid frame size: payload=%u len=%d", payloadSize, len);
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
      ESP_LOGD(TAG, "Ignoring packet from unknown sender");
      return;
    }

    if (!activeInstance->addMasterPeer(recv_info->src_addr, currentChannel, millis())) {
      return;
    }
  } else {
    activeInstance->masters[knownIndex].lastSeenMs = millis();
    activeInstance->masters[knownIndex].channel = currentChannel;
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
      if (payloadSize > 0) {
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::IdentityReq,
                                                      sizeof(app::espnow::state_binary::IdentityReqCommand))) {
          sendIdentityStateNow(*activeInstance);
          sendFeaturesStateNow(*activeInstance);
          break;
        }
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::WifiCredentials,
                                                      sizeof(app::espnow::state_binary::WifiCredentialsCommand))) {
          const auto* command = reinterpret_cast<const app::espnow::state_binary::WifiCredentialsCommand*>(payload);
          char ssid[sizeof(command->ssid) + 1] = {0};
          char password[sizeof(command->password) + 1] = {0};
          memcpy(ssid, command->ssid, sizeof(command->ssid));
          memcpy(password, command->password, sizeof(command->password));

          if (!app::network::wifiManager.requestConnect(ssid, password)) {
            ESP_LOGW(TAG, "WiFi credentials command rejected");
          }
          break;
        }
      } else {
        ESP_LOGI(TAG, "Command packet received (empty)");
      }
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
