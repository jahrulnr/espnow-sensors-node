#pragma once

#if defined(PROFILE_INCLUDE_PATH)
#include PROFILE_INCLUDE_PATH
#elif defined(BOARD_PROFILE_ESP32_CAM)
#include "profiles/profile_esp32_cam.h"
#elif defined(BOARD_PROFILE_ESP32_S3)
#include "profiles/profile_esp32_s3.h"
#elif defined(BOARD_PROFILE_ESP32_C3)
#include "profiles/profile_esp32_c3.h"
#else
// Default profile keeps current project behavior on ESP32-C3.
#include "profiles/profile_esp32_c3.h"
#endif
