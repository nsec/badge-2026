#pragma once

#include <Stream.h>

namespace challenges {
namespace example {

void init();

/**
 * Handle the 'example' command
 */
void handleExampleCommand(Stream &stream);

}  // namespace example
}  // namespace challenges
