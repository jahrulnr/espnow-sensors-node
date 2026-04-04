#pragma once

#include <Arduino.h>

namespace app::espnow::state_binary {

static constexpr uint8_t kMagic = 0xB1;
static constexpr uint8_t kVersion = 1;

enum class Type : uint8_t {
  Identity = 1,
  Sensor = 2,
  ProxyReq = 3,
  Reserved4 = 4,
  MasterNet = 5,
  SlaveAlive = 6,
  ProxyRespChunk = 7,
  Reserved8 = 8,
  Features = 9,
  IdentityReq = 10,
  Mmwave = 11,
  WifiCredentials = 12,
  ServoControl = 13,
  ServoAck = 14,
  ModuleListReq = 15,
  ModuleInfo = 16,
  CameraCapture = 17,
  WifiKeyExchange = 18,
  WifiCredentialsSecure = 19,
  MmwaveRangeConfig = 20,
  WifiWsEndpoint = 21,
};

enum class ModuleDomain : uint8_t {
  Sensor = 1,
  Actuator = 2,
};

enum Feature : uint32_t {
  FeatureIdentity = 1UL << 0,
  FeatureSensor = 1UL << 1,
  FeatureReserved2 = 1UL << 2,
  FeatureProxyClient = 1UL << 3,
  FeatureCameraJpeg = 1UL << 4,
  FeatureCameraStream = 1UL << 5,
  FeatureControlBasic = 1UL << 6,
  FeatureMmwave = 1UL << 7,
  FeatureWifiSta = 1UL << 8,
  FeatureActuationServo = 1UL << 9,
};

enum class HttpMethod : uint8_t {
  Get = 1,
  Post = 2,
  Patch = 3,
};

struct __attribute__((packed)) Header {
  uint8_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t reserved;
};

struct __attribute__((packed)) IdentityState {
  Header header;
  char id[24];
};

struct __attribute__((packed)) SensorState {
  Header header;
  int16_t temperature10;
  uint16_t humidity10;
};

struct __attribute__((packed)) ProxyReqState {
  Header header;
  uint8_t method;
  char url[140];
};

struct __attribute__((packed)) MasterNetState {
  Header header;
  uint8_t online;
  uint8_t channel;
};

struct __attribute__((packed)) SlaveAliveState {
  Header header;
};

struct __attribute__((packed)) FeaturesState {
  Header header;
  uint32_t featureBits;
  uint16_t contractVersion;
  uint16_t reserved;
};

struct __attribute__((packed)) MmwaveState {
  Header header;
  uint8_t detected;
  uint8_t hasDistance;
  uint16_t distanceCm;
  uint16_t frameCount;
  uint16_t byteCount;
  uint8_t targetState;
  uint8_t reportType;
};

static constexpr size_t kProxyChunkDataBytes = 160;

struct __attribute__((packed)) ProxyRespChunkCommand {
  Header header;
  uint16_t requestId;
  uint16_t idx;
  uint16_t total;
  uint8_t ok;
  int16_t code;
  uint8_t dataLen;
  uint8_t data[kProxyChunkDataBytes];
};

struct __attribute__((packed)) IdentityReqCommand {
  Header header;
};

struct __attribute__((packed)) WifiCredentialsCommand {
  Header header;
  char ssid[32];
  char password[64];
};

struct __attribute__((packed)) ServoControlCommand {
  Header header;
  char group[16];
  uint8_t channel;
  uint16_t targetDeg10;
  uint16_t transitionMs;
};

struct __attribute__((packed)) ServoAckState {
  Header header;
  uint8_t ok;
  uint8_t status;
  char group[16];
  uint8_t channel;
  uint16_t targetDeg10;
  uint16_t appliedDeg10;
  uint32_t timestampMs;
};

struct __attribute__((packed)) ModuleListReqCommand {
  Header header;
};

struct __attribute__((packed)) ModuleInfoState {
  Header header;
  uint8_t index;
  uint8_t total;
  uint8_t domain;
  uint8_t reserved0;
  uint32_t featureBits;
  char id[16];
};

struct __attribute__((packed)) CameraCaptureState {
  Header header;
  uint16_t width;
  uint16_t height;
  uint32_t frameBytes;
  uint16_t latencyMs;
  uint8_t frameFormat;
  uint8_t cameraType;
};

static constexpr uint8_t kWifiKeyCurveSecp256r1 = 1;
static constexpr size_t kWifiKeyExchangePublicKeyBytes = 33;
static constexpr size_t kWifiCredentialsNonceBytes = 12;
static constexpr size_t kWifiCredentialsCiphertextBytes = 98;
static constexpr size_t kWifiCredentialsTagBytes = 16;

struct __attribute__((packed)) WifiKeyExchangeState {
  Header header;
  uint32_t keyId;
  uint8_t curve;
  uint8_t publicKeySize;
  uint8_t publicKey[kWifiKeyExchangePublicKeyBytes];
};

struct __attribute__((packed)) WifiCredentialsSecureCommand {
  Header header;
  uint32_t keyId;
  uint32_t counter;
  uint8_t ephemeralKeySize;
  uint8_t ephemeralPublicKey[kWifiKeyExchangePublicKeyBytes];
  uint8_t nonce[kWifiCredentialsNonceBytes];
  uint8_t ciphertext[kWifiCredentialsCiphertextBytes];
  uint8_t tag[kWifiCredentialsTagBytes];
};

struct __attribute__((packed)) MmwaveRangeConfigCommand {
  Header header;
  uint16_t maxDistanceCm;
  uint8_t persistToNvs;
  uint8_t reserved0;
};

static constexpr size_t kWifiWsPathBytes = 24;
static constexpr size_t kWifiWsHostnameBytes = 32;

struct __attribute__((packed)) WifiWsEndpointState {
  Header header;
  uint8_t connected;
  uint8_t ip[4];
  uint16_t port;
  char path[kWifiWsPathBytes];
  char hostname[kWifiWsHostnameBytes];
};

inline void initHeader(Header& header, Type type) {
  header.magic = kMagic;
  header.version = kVersion;
  header.type = static_cast<uint8_t>(type);
  header.reserved = 0;
}

inline bool hasValidHeader(const uint8_t* payload, size_t payloadSize) {
  if (payload == nullptr || payloadSize < sizeof(Header)) {
    return false;
  }

  const auto* header = reinterpret_cast<const Header*>(payload);
  return header->magic == kMagic && header->version == kVersion;
}

inline bool hasTypeAndSize(const uint8_t* payload, size_t payloadSize, Type expectedType, size_t expectedSize) {
  if (!hasValidHeader(payload, payloadSize) || payloadSize != expectedSize) {
    return false;
  }

  const auto* header = reinterpret_cast<const Header*>(payload);
  return header->type == static_cast<uint8_t>(expectedType);
}

}  // namespace app::espnow::state_binary
