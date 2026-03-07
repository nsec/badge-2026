#pragma once

#include <cstdint>

namespace core {
namespace storage {

static constexpr uint8_t CONFIG_NAME_MAX_LEN = 64;

struct UserConfig {
  char name[CONFIG_NAME_MAX_LEN + 1] = {0};

  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } favoriteColor{0, 128, 0};  // default: green

  uint8_t brightness = 128;  // default: 50%
};

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
