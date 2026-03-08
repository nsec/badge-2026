#pragma once

#include <Stream.h>

namespace conference {
namespace schedule {

/**
 * Initialize the schedule module
 */
void init();

/**
 * Register CLI commands for this module
 */
void registerCommands();

/**
 * Handle the 'schedule' command
 */
void handleScheduleCommand(Stream &stream);

}  // namespace schedule
}  // namespace conference
