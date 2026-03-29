#pragma once

#include <cstdint>

#include "storage/nvs_contacts.h"

namespace core {
namespace storage {

struct UserConfig {
  ContactProfile profile;
  uint8_t brightness = 128;  // default: 50%
  bool share = false;
};

// Default color applied when no color has been saved to NVS yet.
inline constexpr uint8_t DEFAULT_COLOR_R = 0;
inline constexpr uint8_t DEFAULT_COLOR_G = 128;
inline constexpr uint8_t DEFAULT_COLOR_B = 0;

/**
 * Initialize NVS namespace for user config.
 * Must be called after socialNvsInit() (which calls nvs_flash_init()).
 */
void configNvsInit();

/**
 * Read the full user config from NVS.
 */
UserConfig configRead();

/**
 * Write the full user config to NVS.
 */
void configWrite(const UserConfig &cfg);

}  // namespace storage
}  // namespace core
