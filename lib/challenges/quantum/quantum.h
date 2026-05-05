#pragma once

#include <Stream.h>
#include <cstdint>
#include <string>

namespace challenges {
namespace quantum {

/// Initialize the quantum challenge module and register its dock handler.
void init();

/// Handle the 'quantum' CLI command (list, set, flag, help).
void handleQuantumCommand(Stream &stream, const std::string &args);

}  // namespace quantum
}  // namespace challenges
