#pragma once

namespace core {

/**
 * Create and start the heartbeat software timer.
 * Toggles the status LED at badge::config::heartbeat::blink_interval.
 */
void heartbeatStart();

}  // namespace core
