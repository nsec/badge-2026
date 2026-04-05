#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "hardware/light_sensor.h"

namespace core {

/**
 * Periodically polls the VEML6040 light sensor (every 5 seconds).
 *
 * Increment/decrement are scaled by distance from threshold (200 lux):
 * - Above threshold: +0.2 to +1.0 per poll (brighter = faster fill)
 *   At threshold: ~106 min to fill. In bright sun: ~21 min.
 * - Below threshold: -0.02 to -0.06 per poll (darker = slightly faster decay)
 *   ~10:1 ratio vs bright-light fill rate.
 *
 * NVS writes are batched every 30 seconds to reduce flash wear.
 */
class LightTask : public Task {
public:
  LightTask() : Task("light", badge::config::tasks::priority_light) {}

protected:
  void run() override;
};

/// Get the most recent light sensor reading (for CLI). Returns false if no reading yet.
bool lightLastReading(hw::LightReading &out);

}  // namespace core
