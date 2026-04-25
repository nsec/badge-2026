#pragma once

#include <string>
#include <vector>

namespace core::animation {

/// Initialize the animation filesystem (SPIFFS on ESP32, no-op on native).
/// Returns true on success.
bool storageInit();

/// List available animation names (filenames without .json extension).
std::vector<std::string> storageList();

/// Read the JSON content of an animation file by name.
/// Returns empty string on failure.
std::string storageRead(const char *name);

}  // namespace core::animation
