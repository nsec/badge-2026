#pragma once

#include <cstdint>

#include <badge_config.h>

#include "rtos/queue.hpp"
#include "storage/nvs_config.h"

namespace core {

// Commands sent TO the portal task (from the controller).
struct PortalCommand {
  enum class Type : uint8_t { Start, Stop };
  Type type;

  union {
    struct {
      char ssid[badge::config::wifi::ssid_max_len + 1];
      char passphrase[badge::config::wifi::passphrase_len + 1];
    } start;
  };
};

// Event sent FROM the portal task back to the controller when the user
// saves new settings via the web UI.
struct ConfigChangedEvent {
  storage::UserConfig config;
};

}  // namespace core
