#pragma once

#include <cstdint>

#include "rtos/queue.hpp"
#include "storage/nvs_config.h"

namespace core {

// Commands sent TO the portal task (from the controller).
enum class PortalCommand : uint8_t {
  Start,
  Stop,
};

// Event sent FROM the portal task back to the controller when the user
// saves new settings via the web UI.
struct ConfigChangedEvent {
  storage::UserConfig config;
};

}  // namespace core
