#pragma once

#include <Stream.h>

namespace conference {
namespace schedule {

/**
 * Initialize the schedule module
 */
void init();

/**
 * Handle the 'schedule' command
 */
void handleScheduleCommand(Stream &stream);

}  // namespace schedule
}  // namespace conference
