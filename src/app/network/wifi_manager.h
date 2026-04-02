#pragma once

#include <Arduino.h>

namespace app::network {

class WifiManager {
 public:
  bool isEnabled() const;
  bool isConnected() const;
  bool isChannelLocked() const;

  bool requestConnect(const char* ssid, const char* password);
  void loop();

 private:
  bool pendingConnect = false;
  bool connecting = false;
  bool loggedConnected = false;
  bool mdnsStarted = false;
  uint32_t connectStartedMs = 0;
  char pendingSsid[33] = {0};
  char pendingPassword[65] = {0};
  char activeHostname[33] = {0};
};

extern WifiManager wifiManager;

}  // namespace app::network
