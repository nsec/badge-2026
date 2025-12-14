#pragma once

#include <Arduino.h>

namespace core {
namespace ota {

enum class BootTarget {
  Factory,
  Ota0,
};

void printBootInfo(Stream &io);

// Set next boot partition. Returns true on success.
bool setNextBoot(BootTarget target, Stream &io);

} // namespace ota
} // namespace core
