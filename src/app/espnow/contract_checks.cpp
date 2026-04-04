#include "protocol.h"
#include "state_binary.h"

#include <type_traits>

namespace app::espnow {

static_assert(PROTOCOL_VERSION == 1, "Protocol version changed");
static_assert(DEFAULT_CHANNEL >= 1, "Default channel must be valid");

static_assert(static_cast<uint8_t>(state_binary::Type::Identity) == 1, "Identity type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::Sensor) == 2, "Sensor type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::MasterNet) == 5, "MasterNet type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::Features) == 9, "Features type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::IdentityReq) == 10, "IdentityReq type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::Mmwave) == 11, "Mmwave type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::WifiCredentials) == 12, "WifiCredentials type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::ServoControl) == 13, "ServoControl type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::ServoAck) == 14, "ServoAck type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::ModuleListReq) == 15, "ModuleListReq type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::ModuleInfo) == 16, "ModuleInfo type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::CameraCapture) == 17, "CameraCapture type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::WifiKeyExchange) == 18, "WifiKeyExchange type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::WifiCredentialsSecure) == 19,
              "WifiCredentialsSecure type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::MmwaveRangeConfig) == 20,
              "MmwaveRangeConfig type changed");
static_assert(static_cast<uint8_t>(state_binary::Type::WifiWsEndpoint) == 21,
              "WifiWsEndpoint type changed");

static_assert(sizeof(state_binary::Header) == 4, "state_binary::Header size changed");
static_assert(sizeof(state_binary::SensorState) <= MAX_PAYLOAD_SIZE, "SensorState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::MmwaveState) <= MAX_PAYLOAD_SIZE, "MmwaveState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::FeaturesState) <= MAX_PAYLOAD_SIZE, "FeaturesState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::WifiCredentialsCommand) <= MAX_PAYLOAD_SIZE,
              "WifiCredentialsCommand exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::ServoControlCommand) <= MAX_PAYLOAD_SIZE,
              "ServoControlCommand exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::ServoAckState) <= MAX_PAYLOAD_SIZE,
              "ServoAckState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::ModuleListReqCommand) <= MAX_PAYLOAD_SIZE,
              "ModuleListReqCommand exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::ModuleInfoState) <= MAX_PAYLOAD_SIZE,
              "ModuleInfoState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::CameraCaptureState) <= MAX_PAYLOAD_SIZE,
              "CameraCaptureState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::WifiKeyExchangeState) <= MAX_PAYLOAD_SIZE,
              "WifiKeyExchangeState exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::WifiCredentialsSecureCommand) <= MAX_PAYLOAD_SIZE,
              "WifiCredentialsSecureCommand exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::MmwaveRangeConfigCommand) <= MAX_PAYLOAD_SIZE,
              "MmwaveRangeConfigCommand exceeds ESP-NOW payload");
static_assert(sizeof(state_binary::WifiWsEndpointState) <= MAX_PAYLOAD_SIZE,
              "WifiWsEndpointState exceeds ESP-NOW payload");

static_assert(std::is_trivially_copyable<state_binary::SensorState>::value,
              "SensorState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::MmwaveState>::value,
              "MmwaveState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::WifiCredentialsCommand>::value,
              "WifiCredentialsCommand must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::ServoControlCommand>::value,
              "ServoControlCommand must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::ServoAckState>::value,
              "ServoAckState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::ModuleListReqCommand>::value,
              "ModuleListReqCommand must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::ModuleInfoState>::value,
              "ModuleInfoState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::CameraCaptureState>::value,
              "CameraCaptureState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::WifiKeyExchangeState>::value,
              "WifiKeyExchangeState must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::WifiCredentialsSecureCommand>::value,
              "WifiCredentialsSecureCommand must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::MmwaveRangeConfigCommand>::value,
              "MmwaveRangeConfigCommand must remain trivially copyable");
static_assert(std::is_trivially_copyable<state_binary::WifiWsEndpointState>::value,
              "WifiWsEndpointState must remain trivially copyable");

}  // namespace app::espnow
