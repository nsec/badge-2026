#include "example_challenge.h"
#include <Arduino.h>
#include <../core/cli/cli.h>

namespace challenges {
namespace example {

void init() {
  // TODO: add challenge init
  registerCommands();
}

void tick() {
  // TODO: add challenge periodic logic
}

void registerCommands() {
    // Register the 'example' command with the CLI
    core::cli::registerCommand("example", "example challenge info", 
        [](Stream& stream, const std::string& args) {
            handleExampleCommand(stream);
        });
}

void handleExampleCommand(Stream& stream) {
    stream.println("=== Example Challenge: Placeholder ===");
    stream.println();
    stream.println("This is a placeholder for an example challenge.");
}

} // namespace example
} // namespace challenges
