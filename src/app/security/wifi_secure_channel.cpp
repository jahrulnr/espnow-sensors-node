#include "app/security/wifi_secure_channel.h"

#include <Preferences.h>
#include <esp_log.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>

#include <cstring>

namespace app::security {

namespace {

constexpr const char* kTag = "WIFI_SEC";
constexpr const char* kPrefsNamespace = "wifi_sec";
constexpr const char* kPrefsPrivKey = "priv";
constexpr const char* kPrefsPubKey = "pub";
constexpr const char* kPrefsKeyId = "key_id";
constexpr const char* kPrefsCtr = "rx_ctr";
constexpr uint8_t kCurveId = app::espnow::state_binary::kWifiKeyCurveSecp256r1;

struct __attribute__((packed)) CredentialPlaintext {
  uint8_t ssidLen;
  uint8_t passwordLen;
  char ssid[32];
  char password[64];
};

void writeLe32(uint32_t value, uint8_t out[4]) {
  out[0] = static_cast<uint8_t>(value & 0xFFU);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
  out[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
  out[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

bool initCtrDrbg(mbedtls_entropy_context& entropy, mbedtls_ctr_drbg_context& ctrDrbg) {
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctrDrbg);
  const char* personalization = "wifi-sec";
  const int rc = mbedtls_ctr_drbg_seed(&ctrDrbg,
                                       mbedtls_entropy_func,
                                       &entropy,
                                       reinterpret_cast<const unsigned char*>(personalization),
                                       strlen(personalization));
  return rc == 0;
}

}  // namespace

WifiSecureChannel wifiSecureChannel;

bool WifiSecureChannel::begin() {
  if (ready_) {
    return true;
  }

  if (!loadFromNvs()) {
    if (!generateAndStore()) {
      ESP_LOGE(kTag, "Failed to initialize secure keypair");
      return false;
    }
  }

  ready_ = true;
  ESP_LOGI(kTag, "Secure channel ready keyId=%lu", static_cast<unsigned long>(keyId_));
  return true;
}

bool WifiSecureChannel::fillKeyExchangeState(app::espnow::state_binary::WifiKeyExchangeState& out) const {
  if (!ready_) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));
  app::espnow::state_binary::initHeader(out.header, app::espnow::state_binary::Type::WifiKeyExchange);
  out.keyId = keyId_;
  out.curve = kCurveId;
  out.publicKeySize = static_cast<uint8_t>(sizeof(publicKey_));
  std::memcpy(out.publicKey, publicKey_, sizeof(publicKey_));
  return true;
}

bool WifiSecureChannel::decryptCredentials(const app::espnow::state_binary::WifiCredentialsSecureCommand& command,
                                           char* outSsid,
                                           size_t outSsidSize,
                                           char* outPassword,
                                           size_t outPasswordSize) {
  if (!ready_ || outSsid == nullptr || outPassword == nullptr || outSsidSize == 0 || outPasswordSize == 0) {
    return false;
  }

  if (command.keyId != keyId_) {
    ESP_LOGW(kTag, "Secure command keyId mismatch");
    return false;
  }

  if (command.ephemeralKeySize != app::espnow::state_binary::kWifiKeyExchangePublicKeyBytes) {
    ESP_LOGW(kTag, "Secure command ephemeral key size invalid=%u", static_cast<unsigned>(command.ephemeralKeySize));
    return false;
  }

  if (command.counter <= lastCounter_) {
    ESP_LOGW(kTag,
             "Secure command replay rejected counter=%lu last=%lu",
             static_cast<unsigned long>(command.counter),
             static_cast<unsigned long>(lastCounter_));
    return false;
  }

  uint8_t sessionKey[32] = {0};
  if (!deriveSessionKey(command.ephemeralPublicKey, command.keyId, command.counter, sessionKey)) {
    ESP_LOGW(kTag, "Failed deriving secure session key");
    return false;
  }

  uint8_t aad[9] = {0};
  aad[0] = static_cast<uint8_t>(app::espnow::state_binary::Type::WifiCredentialsSecure);
  writeLe32(command.keyId, &aad[1]);
  writeLe32(command.counter, &aad[5]);

  CredentialPlaintext plaintext{};
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int rc = mbedtls_gcm_setkey(&gcm,
                              MBEDTLS_CIPHER_ID_AES,
                              sessionKey,
                              static_cast<unsigned int>(sizeof(sessionKey) * 8U));
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(
        &gcm,
        app::espnow::state_binary::kWifiCredentialsCiphertextBytes,
        command.nonce,
        app::espnow::state_binary::kWifiCredentialsNonceBytes,
        aad,
        sizeof(aad),
        command.tag,
        app::espnow::state_binary::kWifiCredentialsTagBytes,
        command.ciphertext,
        reinterpret_cast<uint8_t*>(&plaintext));
  }

  mbedtls_gcm_free(&gcm);
  std::memset(sessionKey, 0, sizeof(sessionKey));

  if (rc != 0) {
    ESP_LOGW(kTag, "Secure command decrypt failed rc=%d", rc);
    return false;
  }

  if (plaintext.ssidLen == 0 || plaintext.ssidLen >= sizeof(plaintext.ssid) ||
      plaintext.passwordLen >= sizeof(plaintext.password)) {
    std::memset(&plaintext, 0, sizeof(plaintext));
    ESP_LOGW(kTag, "Secure command plaintext length invalid");
    return false;
  }

  if (plaintext.ssidLen + 1 > outSsidSize || plaintext.passwordLen + 1 > outPasswordSize) {
    std::memset(&plaintext, 0, sizeof(plaintext));
    ESP_LOGW(kTag, "Secure command output buffers too small");
    return false;
  }

  std::memcpy(outSsid, plaintext.ssid, plaintext.ssidLen);
  outSsid[plaintext.ssidLen] = '\0';
  std::memcpy(outPassword, plaintext.password, plaintext.passwordLen);
  outPassword[plaintext.passwordLen] = '\0';

  std::memset(&plaintext, 0, sizeof(plaintext));

  lastCounter_ = command.counter;
  if (!storeCounterToNvs(lastCounter_)) {
    ESP_LOGW(kTag, "Failed persisting secure counter");
  }

	ESP_LOGI(kTag, "WiFi credential received: %s", plaintext.ssid);
  return true;
}

bool WifiSecureChannel::loadFromNvs() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }

  const size_t privLen = prefs.getBytesLength(kPrefsPrivKey);
  const size_t pubLen = prefs.getBytesLength(kPrefsPubKey);
  const uint32_t savedKeyId = prefs.getUInt(kPrefsKeyId, 0);
  const uint32_t savedCtr = prefs.getUInt(kPrefsCtr, 0);

  if (privLen != sizeof(privateKey_) || pubLen != sizeof(publicKey_) || savedKeyId == 0) {
    prefs.end();
    return false;
  }

  if (prefs.getBytes(kPrefsPrivKey, privateKey_, sizeof(privateKey_)) != sizeof(privateKey_) ||
      prefs.getBytes(kPrefsPubKey, publicKey_, sizeof(publicKey_)) != sizeof(publicKey_)) {
    prefs.end();
    return false;
  }

  keyId_ = savedKeyId;
  lastCounter_ = savedCtr;
  prefs.end();
  return true;
}

bool WifiSecureChannel::generateAndStore() {
  mbedtls_ecp_group grp;
  mbedtls_mpi d;
  mbedtls_ecp_point q;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;

  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&q);

  if (!initCtrDrbg(entropy, ctrDrbg)) {
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ctr_drbg_free(&ctrDrbg);
    mbedtls_entropy_free(&entropy);
    return false;
  }

  int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
  if (rc == 0) {
    rc = mbedtls_ecp_gen_keypair(&grp, &d, &q, mbedtls_ctr_drbg_random, &ctrDrbg);
  }

  size_t pubOut = 0;
  if (rc == 0) {
    rc = mbedtls_mpi_write_binary(&d, privateKey_, sizeof(privateKey_));
  }
  if (rc == 0) {
    rc = mbedtls_ecp_point_write_binary(&grp,
                                        &q,
                                        MBEDTLS_ECP_PF_COMPRESSED,
                                        &pubOut,
                                        publicKey_,
                                        sizeof(publicKey_));
  }

  uint8_t keyHash[32] = {0};
  if (rc == 0 && pubOut == sizeof(publicKey_)) {
    mbedtls_sha256(publicKey_, sizeof(publicKey_), keyHash, 0);
    keyId_ = static_cast<uint32_t>(keyHash[0]) |
             (static_cast<uint32_t>(keyHash[1]) << 8) |
             (static_cast<uint32_t>(keyHash[2]) << 16) |
             (static_cast<uint32_t>(keyHash[3]) << 24);
  } else {
    rc = -1;
  }

  std::memset(keyHash, 0, sizeof(keyHash));

  mbedtls_ecp_point_free(&q);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  mbedtls_ctr_drbg_free(&ctrDrbg);
  mbedtls_entropy_free(&entropy);

  if (rc != 0 || keyId_ == 0) {
    std::memset(privateKey_, 0, sizeof(privateKey_));
    std::memset(publicKey_, 0, sizeof(publicKey_));
    keyId_ = 0;
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }

  bool ok = prefs.putBytes(kPrefsPrivKey, privateKey_, sizeof(privateKey_)) == sizeof(privateKey_);
  ok = prefs.putBytes(kPrefsPubKey, publicKey_, sizeof(publicKey_)) == sizeof(publicKey_) && ok;
  ok = prefs.putUInt(kPrefsKeyId, keyId_) == keyId_ && ok;
  lastCounter_ = 0;
  ok = prefs.putUInt(kPrefsCtr, lastCounter_) == lastCounter_ && ok;
  prefs.end();

  return ok;
}

bool WifiSecureChannel::storeCounterToNvs(uint32_t counter) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }

  const bool ok = prefs.putUInt(kPrefsCtr, counter) == counter;
  prefs.end();
  return ok;
}

bool WifiSecureChannel::deriveSessionKey(
    const uint8_t peerPublicKey[app::espnow::state_binary::kWifiKeyExchangePublicKeyBytes],
    uint32_t keyId,
    uint32_t counter,
    uint8_t outKey[32]) const {
  if (peerPublicKey == nullptr || outKey == nullptr) {
    return false;
  }

  mbedtls_ecp_group grp;
  mbedtls_ecp_point qPeer;
  mbedtls_mpi d;
  mbedtls_mpi z;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;

  mbedtls_ecp_group_init(&grp);
  mbedtls_ecp_point_init(&qPeer);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&z);

  if (!initCtrDrbg(entropy, ctrDrbg)) {
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&qPeer);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ctr_drbg_free(&ctrDrbg);
    mbedtls_entropy_free(&entropy);
    return false;
  }

  int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
  if (rc == 0) {
    rc = mbedtls_mpi_read_binary(&d, privateKey_, sizeof(privateKey_));
  }
  if (rc == 0) {
    rc = mbedtls_ecp_point_read_binary(&grp,
                                       &qPeer,
                                       peerPublicKey,
                                       app::espnow::state_binary::kWifiKeyExchangePublicKeyBytes);
  }
  if (rc == 0) {
    rc = mbedtls_ecdh_compute_shared(&grp, &z, &qPeer, &d, mbedtls_ctr_drbg_random, &ctrDrbg);
  }

  uint8_t shared[32] = {0};
  if (rc == 0) {
    rc = mbedtls_mpi_write_binary(&z, shared, sizeof(shared));
  }

  if (rc == 0) {
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, shared, sizeof(shared));

    uint8_t keyIdBytes[4] = {0};
    uint8_t counterBytes[4] = {0};
    writeLe32(keyId, keyIdBytes);
    writeLe32(counter, counterBytes);
    static constexpr uint8_t label[] = {'f', 'h', '-', 'w', 'i', 'f', 'i', '-', 'v', '1'};
    mbedtls_sha256_update(&sha, keyIdBytes, sizeof(keyIdBytes));
    mbedtls_sha256_update(&sha, counterBytes, sizeof(counterBytes));
    mbedtls_sha256_update(&sha, label, sizeof(label));
    mbedtls_sha256_finish(&sha, outKey);
    mbedtls_sha256_free(&sha);
  }

  std::memset(shared, 0, sizeof(shared));

  mbedtls_mpi_free(&z);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_point_free(&qPeer);
  mbedtls_ecp_group_free(&grp);
  mbedtls_ctr_drbg_free(&ctrDrbg);
  mbedtls_entropy_free(&entropy);

  return rc == 0;
}

}  // namespace app::security
