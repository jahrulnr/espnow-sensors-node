#include "slave_runtime_support.h"

#include "protocol.h"

#include <app_config.h>

#include <Preferences.h>
#include <cstring>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

namespace app::espnow::runtime {

namespace {

static constexpr uint8_t MIN_SCAN_CHANNEL = 1;
static constexpr uint8_t MAX_SCAN_CHANNEL = 13;
static constexpr uint8_t CACHE_VERSION = 1;
static constexpr size_t CACHE_MAX_ENTRIES = NODE_MASTER_CACHE_MAX_ENTRIES;
static constexpr const char* CACHE_PREFS_NAMESPACE = "espn_cache";
static constexpr const char* CACHE_PREFS_KEY = "master_tbl";

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
RTC_DATA_ATTR uint8_t gCachedScanHopCount = 0;
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

void persistMasterCacheToNvs(const MasterCacheStore& cache, const char* logTag) {
  Preferences prefs;
  if (!prefs.begin(CACHE_PREFS_NAMESPACE, false)) {
    ESP_LOGW(logTag, "Failed opening NVS namespace '%s'", CACHE_PREFS_NAMESPACE);
    return;
  }

  const size_t written = prefs.putBytes(CACHE_PREFS_KEY, &cache, sizeof(cache));
  if (written != sizeof(cache)) {
    ESP_LOGW(logTag, "Failed writing master cache to NVS (written=%u)", static_cast<unsigned>(written));
  }
  prefs.end();
}

bool loadMasterCacheFromNvs(MasterCacheStore& cache, const char* logTag) {
  Preferences prefs;
  if (!prefs.begin(CACHE_PREFS_NAMESPACE, true)) {
    ESP_LOGW(logTag, "Failed opening NVS namespace '%s' (read)", CACHE_PREFS_NAMESPACE);
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

void ensureMasterCacheLoaded(const char* logTag) {
  if (gMasterCacheReady) {
    return;
  }

  if (!loadMasterCacheFromNvs(gMasterCache, logTag) || gMasterCache.version != CACHE_VERSION) {
    initMasterCacheDefaults(gMasterCache);
    persistMasterCacheToNvs(gMasterCache, logTag);
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
  gCachedScanHopCount = 0;
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

bool shouldUseCachedScanOnly(const char* logTag) {
  ensureMasterCacheLoaded(logTag);
  const uint8_t intervalWakeCycles = fullScanWakeInterval();
  gWakeCyclesSinceFullScan++;

  if (gWakeCyclesSinceFullScan >= intervalWakeCycles) {
    gWakeCyclesSinceFullScan = 0;
    ESP_LOGI(logTag, "Cache TTL reached, forcing full scan this wake cycle");
    return false;
  }

  return gMasterCache.entryCount > 0;
}

}  // namespace

bool ensureWifiStaReady(const char* logTag) {
  ESP_LOGI(logTag, "begin(): init WiFi STA core");

  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(logTag, "esp_netif_init failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(logTag, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
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
    ESP_LOGE(logTag, "esp_wifi_init failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
    ESP_LOGE(logTag, "esp_wifi_set_storage failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(logTag, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_start();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
    ESP_LOGE(logTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_wifi_disconnect();
  return true;
}

uint8_t chooseInitialScanChannel(uint8_t requestedChannel, const char* logTag) {
  ensureMasterCacheLoaded(logTag);
  const bool useCachedScanOnly = shouldUseCachedScanOnly(logTag);
  buildCachedScanPlan(useCachedScanOnly);

  if (gCachedScanChannelCount > 0) {
    const uint8_t startChannel = gCachedScanChannels[0];
    ESP_LOGI(logTag,
             "Using cached scan channel %u for fast wake link (max hops=%u)",
             startChannel,
             static_cast<unsigned>(NODE_MASTER_CACHE_FAST_SCAN_MAX_HOPS));
    return startChannel;
  }

  return requestedChannel;
}

uint8_t chooseNextScanChannel(uint8_t currentChannel) {
  if (gUseCachedScanMode && gCachedScanChannelCount > 0) {
    if (NODE_MASTER_CACHE_FAST_SCAN_MAX_HOPS > 0 &&
        gCachedScanHopCount >= NODE_MASTER_CACHE_FAST_SCAN_MAX_HOPS) {
      gUseCachedScanMode = false;
      gCachedScanIndex = 0;
    } else {
      const uint8_t nextChannel = gCachedScanChannels[gCachedScanIndex];
      gCachedScanIndex = static_cast<uint8_t>((gCachedScanIndex + 1) % gCachedScanChannelCount);
      if (gCachedScanHopCount < 255) {
        gCachedScanHopCount++;
      }
      return nextChannel;
    }
  }

  if (gCachedScanHopCount > 0) {
    gCachedScanHopCount = 0;
  }

  if (currentChannel < MIN_SCAN_CHANNEL || currentChannel >= MAX_SCAN_CHANNEL) {
    return MIN_SCAN_CHANNEL;
  }

  return static_cast<uint8_t>(currentChannel + 1);
}

void updateMasterCacheEntry(const uint8_t mac[6], uint8_t channel) {
  if (mac == nullptr) {
    return;
  }

  ensureMasterCacheLoaded("espnow_slave");
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

  gMasterCache.entries[0].used = 1;
  memcpy(gMasterCache.entries[0].mac, mac, 6);
  gMasterCache.entries[0].channel = normalizedChannel;
  gMasterCache.entries[0].reserved = 0;
  recountMasterCacheEntries(gMasterCache);
}

void applyWakeResultToCache(bool linked, const char* logTag) {
  ensureMasterCacheLoaded(logTag);

  if (linked) {
    gMasterCache.missStreak = 0;
    persistMasterCacheToNvs(gMasterCache, logTag);
    return;
  }

  if (gMasterCache.missStreak < 255) {
    gMasterCache.missStreak++;
  }

  if (gMasterCache.missStreak > NODE_MASTER_CACHE_INVALID_AFTER_WAKE_MISS) {
    ESP_LOGW(logTag,
             "Invalidating master cache after %u wake misses",
             static_cast<unsigned>(gMasterCache.missStreak));
    initMasterCacheDefaults(gMasterCache);
  }

  persistMasterCacheToNvs(gMasterCache, logTag);
}

}  // namespace app::espnow::runtime