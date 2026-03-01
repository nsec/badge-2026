#include "schedule.h"
#include <Arduino.h>
#include <string>
#include <../core/tasks/cli.h>

namespace conference {
namespace schedule {

void init() {
  Serial.println("  - Schedule module loaded");
  registerCommands();
}

void registerCommands() {
  // Register the 'schedule' command with the CLI
  core::cli::registerCommand("schedule", "show conference schedule", [](Stream &stream, const std::string &args) {
    handleScheduleCommand(stream);
  });
}

void handleScheduleCommand(Stream &stream) {
  stream.println("=== NorthSec 2026 Schedule ===");
  stream.println();
  stream.println("Friday, May 16:");
  stream.println("  10:00 - Opening Ceremonies");
  stream.println("  11:00 - CTF Begins");
  stream.println("  14:00 - Workshop: Hardware Hacking");
  stream.println();
  stream.println("Saturday, May 17:");
  stream.println("  09:00 - Badge Challenge Unlocks");
  stream.println("  13:00 - Networking Social");
  stream.println("  19:00 - Evening Events");
  stream.println();
  stream.println("Sunday, May 18:");
  stream.println("  10:00 - Final CTF Push");
  stream.println("  16:00 - Closing & Awards");
  stream.println();
  stream.println("Visit https://nsec.io for full schedule");
}

}  // namespace schedule
}  // namespace conference
