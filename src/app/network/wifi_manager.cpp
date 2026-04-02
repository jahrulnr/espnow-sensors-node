#include "wifi_manager.h"

#include "websocket_gateway.h"

#include <app_config.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_log.h>

namespace app::network {

namespace {

static constexpr const char* TAG = "WIFI_MGR";

void sanitizeHostname(char* hostname, size_t capacity) {
  if (hostname == nullptr || capacity == 0) {
    return;
  }

  for (size_t i = 0; i < capacity && hostname[i] != '\0'; ++i) {
    const char c = hostname[i];
    const bool isLower = c >= 'a' && c <= 'z';
    const bool isUpper = c >= 'A' && c <= 'Z';
    const bool isDigit = c >= '0' && c <= '9';
    if (!isLower && !isUpper && !isDigit && c != '-') {
      hostname[i] = '-';
    }
  }
}

}  // namespace

WifiManager wifiManager;

bool WifiManager::isEnabled() const {
#if ENABLE_WIFI_MODE
  return true;
#else
  return false;
#endif
}

bool WifiManager::isConnected() const {
#if ENABLE_WIFI_MODE
  return WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

bool WifiManager::isChannelLocked() const {
#if ENABLE_WIFI_MODE
  return connecting || isConnected();
#else
  return false;
#endif
}

bool WifiManager::requestConnect(const char* ssid, const char* password) {
#if !ENABLE_WIFI_MODE
  (void)ssid;
  (void)password;
  ESP_LOGW(TAG, "WiFi mode disabled, credentials ignored");
  return false;
#else
  if (ssid == nullptr || ssid[0] == '\0') {
    ESP_LOGW(TAG, "WiFi credentials rejected: empty SSID");
    return false;
  }

  strncpy(pendingSsid, ssid, sizeof(pendingSsid) - 1);
  pendingSsid[sizeof(pendingSsid) - 1] = '\0';

  if (password == nullptr) {
    pendingPassword[0] = '\0';
  } else {
    strncpy(pendingPassword, password, sizeof(pendingPassword) - 1);
    pendingPassword[sizeof(pendingPassword) - 1] = '\0';
  }

  pendingConnect = true;
  connecting = false;
  loggedConnected = false;

  ESP_LOGI(TAG, "WiFi connect request queued for SSID '%s'", pendingSsid);
  return true;
#endif
}

void WifiManager::loop() {
#if !ENABLE_WIFI_MODE
  return;
#else
  const bool connectedNow = WiFi.status() == WL_CONNECTED;
  if (connectedNow != wasConnected) {
#if ENABLE_WEBSOCKET_GATEWAY
    websocketGateway.onWifiConnectionChanged(connectedNow);
#endif
    wasConnected = connectedNow;
  }

#if ENABLE_WEBSOCKET_GATEWAY
  if (connectedNow) {
    websocketGateway.loop();
  }
#endif

  if (mdnsStarted && WiFi.status() != WL_CONNECTED) {
    MDNS.end();
    mdnsStarted = false;
  }

  if (pendingConnect) {
    pendingConnect = false;
    connecting = true;
    loggedConnected = false;
    mdnsStarted = false;
    connectStartedMs = millis();

    strncpy(activeHostname, WIFI_CLIENT_HOSTNAME, sizeof(activeHostname) - 1);
    activeHostname[sizeof(activeHostname) - 1] = '\0';
    sanitizeHostname(activeHostname, sizeof(activeHostname));
    if (activeHostname[0] == '\0') {
      strncpy(activeHostname, "espnow-node", sizeof(activeHostname) - 1);
      activeHostname[sizeof(activeHostname) - 1] = '\0';
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(activeHostname);
    WiFi.setAutoReconnect(true);
    WiFi.begin(pendingSsid, pendingPassword);
    ESP_LOGI(TAG, "Connecting WiFi SSID '%s' with hostname '%s'", pendingSsid, activeHostname);
  }

  if (!connecting) {
    return;
  }

  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (!loggedConnected) {
      const IPAddress ip = WiFi.localIP();
      ESP_LOGI(TAG, "WiFi connected SSID '%s' IP=%u.%u.%u.%u",
               WiFi.SSID().c_str(),
               static_cast<unsigned>(ip[0]),
               static_cast<unsigned>(ip[1]),
               static_cast<unsigned>(ip[2]),
               static_cast<unsigned>(ip[3]));
      if (!mdnsStarted) {
        if (MDNS.begin(activeHostname)) {
          mdnsStarted = true;
          ESP_LOGI(TAG, "mDNS active at '%s.local'", activeHostname);
        } else {
          ESP_LOGW(TAG, "Failed to start mDNS for '%s.local'", activeHostname);
        }
      }
      loggedConnected = true;
    }
    connecting = false;
    return;
  }

  if ((millis() - connectStartedMs) > WIFI_CONNECT_TIMEOUT_MS) {
    ESP_LOGW(TAG, "WiFi connect timeout after %u ms", static_cast<unsigned>(WIFI_CONNECT_TIMEOUT_MS));
    connecting = false;
  }
#endif
}

}  // namespace app::network
