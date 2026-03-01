#pragma once

#include <Stream.h>

namespace challenges {
namespace example {

void init();
void tick();

/**
 * Register CLI commands for this module
 */
void registerCommands();

/**
 * Handle the 'example' command
 */
void handleExampleCommand(Stream &stream);

}  // namespace example
}  // namespace challenges
