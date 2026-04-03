#include "slave.h"

#include "state_binary.h"

#include "app/actuation/actuator_manager.h"
#include "app/network/wifi_manager.h"
#include "app/power/sleep_guard.h"
#include "app/sensing/sensor_manager.h"
#include <app_config.h>
#include <WiFi.h>
#include <Preferences.h>
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
                   | app::sensing::sensorManager.featureBits()
                   | app::actuation::actuatorManager.featureBits();
#if ENABLE_WIFI_MODE
  state.featureBits |= static_cast<uint32_t>(app::espnow::state_binary::FeatureWifiSta);
#endif

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("espnow_slave", "Failed sending feature state");
  }
  return sent;
}

void sendServoAckNow(SlaveNode& node, const app::actuation::ActuationResponse& response) {
  app::espnow::state_binary::ServoAckState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::ServoAck);
  state.ok = response.ok ? 1 : 0;
  state.status = static_cast<uint8_t>(response.status);
  strncpy(state.group, response.servo.group, sizeof(state.group) - 1);
  state.group[sizeof(state.group) - 1] = '\0';
  state.channel = response.servo.channel;
  state.targetDeg10 = response.servo.targetDeg10;
  state.appliedDeg10 = response.servo.appliedDeg10;
  state.timestampMs = response.timestampMs;

  if (!node.sendStateBinary(&state, sizeof(state))) {
    ESP_LOGW("espnow_slave", "Failed sending servo ack state");
  }
}

bool sendModuleInfoNow(SlaveNode& node,
                       uint8_t index,
                       uint8_t total,
                       app::espnow::state_binary::ModuleDomain domain,
                       const char* id,
                       uint32_t featureBits) {
  app::espnow::state_binary::ModuleInfoState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::ModuleInfo);
  state.index = index;
  state.total = total;
  state.domain = static_cast<uint8_t>(domain);
  state.reserved0 = 0;
  state.featureBits = featureBits;
  if (id != nullptr) {
    strncpy(state.id, id, sizeof(state.id) - 1);
    state.id[sizeof(state.id) - 1] = '\0';
  }

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("espnow_slave", "Failed sending module info state");
  }

  return sent;
}

bool sendModuleListNow(SlaveNode& node) {
  app::sensing::SensorManager::ModuleDescriptor sensorDescriptors[app::sensing::SensorManager::MAX_MODULES] = {};
  app::actuation::ActuatorManager::ModuleDescriptor
      actuatorDescriptors[app::actuation::ActuatorManager::MAX_MODULES] = {};

  const size_t sensorCount = app::sensing::sensorManager.listModules(sensorDescriptors, app::sensing::SensorManager::MAX_MODULES);
  const size_t actuatorCount =
      app::actuation::actuatorManager.listModules(actuatorDescriptors, app::actuation::ActuatorManager::MAX_MODULES);
  const size_t totalCount = sensorCount + actuatorCount;
  const uint8_t total = totalCount > 255 ? 255 : static_cast<uint8_t>(totalCount);

  bool sentAny = false;
  uint8_t index = 0;
  for (size_t i = 0; i < sensorCount && index < total; ++i) {
    sentAny = sendModuleInfoNow(node,
                                index,
                                total,
                                app::espnow::state_binary::ModuleDomain::Sensor,
                                sensorDescriptors[i].id,
                                sensorDescriptors[i].featureBits) || sentAny;
    index++;
  }

  for (size_t i = 0; i < actuatorCount && index < total; ++i) {
    sentAny = sendModuleInfoNow(node,
                                index,
                                total,
                                app::espnow::state_binary::ModuleDomain::Actuator,
                                actuatorDescriptors[i].id,
                                actuatorDescriptors[i].featureBits) || sentAny;
    index++;
  }

  return sentAny;
}

}  // namespace

static const char* TAG = "espnow_slave";
static constexpr uint8_t MIN_SCAN_CHANNEL = 1;
static constexpr uint8_t MAX_SCAN_CHANNEL = 13;
static constexpr uint32_t CHANNEL_SCAN_INTERVAL_MS = NODE_SCAN_CHANNEL_DWELL_MS;
static constexpr uint32_t MASTER_TIMEOUT_MS = 12000;
static constexpr uint32_t HELLO_INTERVAL_MS = 7000;
static constexpr uint8_t CACHE_VERSION = 1;
static constexpr size_t CACHE_MAX_ENTRIES = NODE_MASTER_CACHE_MAX_ENTRIES;
static constexpr const char* CACHE_PREFS_NAMESPACE = "espn_cache";
static constexpr const char* CACHE_PREFS_KEY = "master_tbl";
static const uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct MasterCacheEntry {
  uint8_t used;
  uint8_t mac[6];
  uint8_t channel;
  uint8_t reserved;
};

struct MasterCacheStore {
  uint8_t version;
  uint8_t missStreak;
  uint8_t entryCount;
  uint8_t reserved;
  MasterCacheEntry entries[CACHE_MAX_ENTRIES];
};

RTC_DATA_ATTR MasterCacheStore gMasterCache = {};
RTC_DATA_ATTR bool gMasterCacheReady = false;
RTC_DATA_ATTR uint8_t gWakeCyclesSinceFullScan = 0;
RTC_DATA_ATTR uint8_t gCachedScanChannels[CACHE_MAX_ENTRIES] = {0};
RTC_DATA_ATTR uint8_t gCachedScanChannelCount = 0;
RTC_DATA_ATTR uint8_t gCachedScanIndex = 0;
RTC_DATA_ATTR bool gUseCachedScanMode = false;

uint8_t clampChannel(uint8_t channel) {
  if (channel < MIN_SCAN_CHANNEL || channel > MAX_SCAN_CHANNEL) {
    return DEFAULT_CHANNEL;
  }
  return channel;
}

void clearMasterCacheEntries(MasterCacheStore& cache) {
  cache.entryCount = 0;
  for (size_t i = 0; i < CACHE_MAX_ENTRIES; ++i) {
    cache.entries[i].used = 0;
    cache.entries[i].channel = DEFAULT_CHANNEL;
    cache.entries[i].reserved = 0;
    memset(cache.entries[i].mac, 0, sizeof(cache.entries[i].mac));
  }
}

void initMasterCacheDefaults(MasterCacheStore& cache) {
  cache.version = CACHE_VERSION;
  cache.missStreak = 0;
  cache.reserved = 0;
  clearMasterCacheEntries(cache);
}

void recountMasterCacheEntries(MasterCacheStore& cache) {
  uint8_t count = 0;
  for (size_t i = 0; i < CACHE_MAX_ENTRIES; ++i) {
    if (cache.entries[i].used != 0) {
      count++;
    }
  }
  cache.entryCount = count;
}

void persistMasterCacheToNvs(const MasterCacheStore& cache) {
  Preferences prefs;
  if (!prefs.begin(CACHE_PREFS_NAMESPACE, false)) {
    ESP_LOGW(TAG, "Failed opening NVS namespace '%s'", CACHE_PREFS_NAMESPACE);
    return;
  }

  const size_t written = prefs.putBytes(CACHE_PREFS_KEY, &cache, sizeof(cache));
  if (written != sizeof(cache)) {
    ESP_LOGW(TAG, "Failed writing master cache to NVS (written=%u)", static_cast<unsigned>(written));
  }
  prefs.end();
}

bool loadMasterCacheFromNvs(MasterCacheStore& cache) {
  Preferences prefs;
  if (!prefs.begin(CACHE_PREFS_NAMESPACE, true)) {
    ESP_LOGW(TAG, "Failed opening NVS namespace '%s' (read)", CACHE_PREFS_NAMESPACE);
    return false;
  }

  const size_t storedSize = prefs.getBytesLength(CACHE_PREFS_KEY);
  if (storedSize != sizeof(cache)) {
    prefs.end();
    return false;
  }

  const size_t read = prefs.getBytes(CACHE_PREFS_KEY, &cache, sizeof(cache));
  prefs.end();
  return read == sizeof(cache);
}

uint8_t fullScanWakeInterval() {
#if ENABLE_POWERSAVE
#if POWERSAVE_SLEEP_MODE == POWERSAVE_SLEEP_MODE_LIGHT
  constexpr uint32_t wakeIntervalMs = POWERSAVE_LIGHT_SLEEP_MS;
#else
  constexpr uint32_t wakeIntervalMs = POWERSAVE_DEEP_SLEEP_SEC * 1000U;
#endif
  const uint32_t interval = NODE_MASTER_CACHE_FULLSCAN_TTL_MS;
  if (wakeIntervalMs == 0 || interval == 0) {
    return 1;
  }

  uint32_t cycles = (interval + wakeIntervalMs - 1U) / wakeIntervalMs;
  if (cycles == 0) {
    cycles = 1;
  }
  if (cycles > 255U) {
    cycles = 255U;
  }
  return static_cast<uint8_t>(cycles);
#else
  return 1;
#endif
}

void ensureMasterCacheLoaded() {
  if (gMasterCacheReady) {
    return;
  }

  if (!loadMasterCacheFromNvs(gMasterCache) || gMasterCache.version != CACHE_VERSION) {
    initMasterCacheDefaults(gMasterCache);
    persistMasterCacheToNvs(gMasterCache);
  } else {
    recountMasterCacheEntries(gMasterCache);
  }

  gMasterCacheReady = true;
}

bool appendChannelIfMissing(uint8_t* channels, size_t& count, uint8_t channel) {
  if (count >= CACHE_MAX_ENTRIES) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (channels[i] == channel) {
      return false;
    }
  }
  channels[count++] = channel;
  return true;
}

void buildCachedScanPlan(bool useCachedScanOnly) {
  gCachedScanChannelCount = 0;
  gCachedScanIndex = 0;
  gUseCachedScanMode = false;
  memset(gCachedScanChannels, 0, sizeof(gCachedScanChannels));

  if (!useCachedScanOnly) {
    return;
  }

  size_t count = 0;
  for (size_t i = 0; i < CACHE_MAX_ENTRIES; ++i) {
    if (gMasterCache.entries[i].used == 0) {
      continue;
    }
    appendChannelIfMissing(gCachedScanChannels, count, clampChannel(gMasterCache.entries[i].channel));
  }

  gCachedScanChannelCount = static_cast<uint8_t>(count);
  gUseCachedScanMode = gCachedScanChannelCount > 0;
}

void updateCacheEntry(const uint8_t mac[6], uint8_t channel) {
  if (mac == nullptr) {
    return;
  }

  ensureMasterCacheLoaded();
  const uint8_t normalizedChannel = clampChannel(channel);

  for (size_t i = 0; i < CACHE_MAX_ENTRIES; ++i) {
    if (gMasterCache.entries[i].used != 0 && memcmp(gMasterCache.entries[i].mac, mac, 6) == 0) {
      gMasterCache.entries[i].channel = normalizedChannel;
      recountMasterCacheEntries(gMasterCache);
      return;
    }
  }

  for (size_t i = 0; i < CACHE_MAX_ENTRIES; ++i) {
    if (gMasterCache.entries[i].used != 0) {
      continue;
    }

    gMasterCache.entries[i].used = 1;
    memcpy(gMasterCache.entries[i].mac, mac, 6);
    gMasterCache.entries[i].channel = normalizedChannel;
    gMasterCache.entries[i].reserved = 0;
    recountMasterCacheEntries(gMasterCache);
    return;
  }

  // Replace oldest slot (index 0) when cache is full.
  gMasterCache.entries[0].used = 1;
  memcpy(gMasterCache.entries[0].mac, mac, 6);
  gMasterCache.entries[0].channel = normalizedChannel;
  gMasterCache.entries[0].reserved = 0;
  recountMasterCacheEntries(gMasterCache);
}

bool shouldUseCachedScanOnly() {
  ensureMasterCacheLoaded();
  const uint8_t intervalWakeCycles = fullScanWakeInterval();
  gWakeCyclesSinceFullScan++;

  if (gWakeCyclesSinceFullScan >= intervalWakeCycles) {
    gWakeCyclesSinceFullScan = 0;
    ESP_LOGI(TAG, "Cache TTL reached, forcing full scan this wake cycle");
    return false;
  }

  return gMasterCache.entryCount > 0;
}

void applyWakeResultToCache(bool linked) {
  ensureMasterCacheLoaded();

  if (linked) {
    gMasterCache.missStreak = 0;
    persistMasterCacheToNvs(gMasterCache);
    return;
  }

  if (gMasterCache.missStreak < 255) {
    gMasterCache.missStreak++;
  }

  if (gMasterCache.missStreak > NODE_MASTER_CACHE_INVALID_AFTER_WAKE_MISS) {
    ESP_LOGW(TAG,
             "Invalidating master cache after %u wake misses",
             static_cast<unsigned>(gMasterCache.missStreak));
    initMasterCacheDefaults(gMasterCache);
  }

  persistMasterCacheToNvs(gMasterCache);
}

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

  ensureMasterCacheLoaded();
  const bool useCachedScanOnly = shouldUseCachedScanOnly();
  buildCachedScanPlan(useCachedScanOnly);

  uint8_t startChannel = channel;
  if (gCachedScanChannelCount > 0) {
    startChannel = gCachedScanChannels[0];
    ESP_LOGI(TAG, "Using cached scan channel %u for fast wake link", startChannel);
  }

  if (startChannel > 0) {
    ESP_LOGI(TAG, "begin(): set initial channel=%u", startChannel);
    esp_err_t channelErr = esp_wifi_set_channel(startChannel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set WiFi channel %d: %s", startChannel, esp_err_to_name(channelErr));
    }
    scanChannel = startChannel;
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

  if (!esp_now_is_peer_exist(kBroadcastMac)) {
    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, kBroadcastMac, 6);
    broadcastPeer.ifidx = WIFI_IF_STA;
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    const esp_err_t addBroadcast = esp_now_add_peer(&broadcastPeer);
    if (addBroadcast != ESP_OK && addBroadcast != ESP_ERR_ESPNOW_EXIST) {
      ESP_LOGW(TAG, "Failed adding broadcast peer: %s", esp_err_to_name(addBroadcast));
    }
  }

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
  if (gUseCachedScanMode && gCachedScanChannelCount > 0) {
    nextChannel = gCachedScanChannels[gCachedScanIndex];
    gCachedScanIndex = static_cast<uint8_t>((gCachedScanIndex + 1) % gCachedScanChannelCount);
  } else {
    if (nextChannel < MIN_SCAN_CHANNEL || nextChannel >= MAX_SCAN_CHANNEL) {
      nextChannel = MIN_SCAN_CHANNEL;
    } else {
      nextChannel++;
    }
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
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    const esp_err_t modErr = esp_now_mod_peer(&peer);
    if (modErr != ESP_OK && modErr != ESP_ERR_ESPNOW_NOT_FOUND) {
      ESP_LOGW(TAG, "Failed to sync master peer channel: %s", esp_err_to_name(modErr));
    }

    masters[existingIndex].channel = channel;
    masters[existingIndex].lastSeenMs = seenMs;
    updateCacheEntry(mac, channel);
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
      ESP_LOGW(TAG, "Failed to sync existing peer channel: %s", esp_err_to_name(modErr));
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
      updateCacheEntry(mac, channel);
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
    updateCacheEntry(mac, channel);
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

void SlaveNode::onWakeCycleLinkResult(bool linked) {
  applyWakeResultToCache(linked);
}

void SlaveNode::markMasterActivity() {
  app::power::touchMasterActivity();
}

bool SlaveNode::sendModuleListSnapshot() {
  if (!started || masterCount == 0) {
    return false;
  }

  return sendModuleListNow(*this);
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
      ESP_LOGW(TAG, "Failed to add peer before send: %s", esp_err_to_name(addErr));
      return false;
    }
  } else if (modErr != ESP_OK) {
    ESP_LOGW(TAG, "Failed to update peer before send: %s", esp_err_to_name(modErr));
  }

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

  bool sent = sendToKnownMasters(PacketType::STATE, payload, payloadSize);

#if NODE_STATE_BROADCAST_MIRROR
  // Mirror state to broadcast as fallback when unicast ACK is unreliable.
  sent = sendToMaster(kBroadcastMac, scanChannel, PacketType::STATE, payload, payloadSize) || sent;
#endif

  return sent;
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
      ESP_LOGW(TAG,
               "TX failed -> %02X:%02X:%02X:%02X:%02X:%02X (ch=%u)",
               tx_info->des_addr[0],
               tx_info->des_addr[1],
               tx_info->des_addr[2],
               tx_info->des_addr[3],
               tx_info->des_addr[4],
               tx_info->des_addr[5],
               static_cast<unsigned>(currentPrimary));
    } else {
      ESP_LOGW(TAG, "TX failed (no tx_info, ch=%u)", static_cast<unsigned>(currentPrimary));
    }
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
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::ServoControl,
                                                      sizeof(app::espnow::state_binary::ServoControlCommand))) {
          const auto* command = reinterpret_cast<const app::espnow::state_binary::ServoControlCommand*>(payload);

          app::actuation::ActuationRequest request = {};
          request.kind = app::actuation::ActuatorKind::Servo;
          request.timestampMs = millis();
          strncpy(request.servo.group, command->group, sizeof(request.servo.group) - 1);
          request.servo.group[sizeof(request.servo.group) - 1] = '\0';
          request.servo.channel = command->channel;
          request.servo.targetDeg10 = command->targetDeg10;
          request.servo.transitionMs = command->transitionMs;

          app::actuation::ActuationResponse response = {};
          app::actuation::actuatorManager.handleRequest(request, response);
          sendServoAckNow(*activeInstance, response);
          break;
        }
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::ModuleListReq,
                                                      sizeof(app::espnow::state_binary::ModuleListReqCommand))) {
          if (!sendModuleListNow(*activeInstance)) {
            ESP_LOGW(TAG, "Module list request received but response send failed");
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
