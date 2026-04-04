#pragma once

#include <cstddef>
#include <cstdint>

#include "app/espnow/state_binary.h"

namespace app::security {

class WifiSecureChannel {
 public:
  bool begin();
  bool fillKeyExchangeState(app::espnow::state_binary::WifiKeyExchangeState& out) const;
  bool decryptCredentials(const app::espnow::state_binary::WifiCredentialsSecureCommand& command,
                          char* outSsid,
                          size_t outSsidSize,
                          char* outPassword,
                          size_t outPasswordSize);

 private:
  static constexpr size_t kPrivateKeyBytes = 32;

  bool loadFromNvs();
  bool generateAndStore();
  bool storeCounterToNvs(uint32_t counter);
  bool deriveSessionKey(const uint8_t peerPublicKey[app::espnow::state_binary::kWifiKeyExchangePublicKeyBytes],
                        uint32_t keyId,
                        uint32_t counter,
                        uint8_t outKey[32]) const;

  bool ready_ = false;
  uint32_t keyId_ = 0;
  uint32_t lastCounter_ = 0;
  uint8_t privateKey_[kPrivateKeyBytes] = {0};
  uint8_t publicKey_[app::espnow::state_binary::kWifiKeyExchangePublicKeyBytes] = {0};
};

extern WifiSecureChannel wifiSecureChannel;

}  // namespace app::security
