#pragma once

// ESP-IDF random number generator stub for native simulator.

#include <cstdint>
#include <cstdlib>

inline uint32_t esp_random() {
  return static_cast<uint32_t>(std::rand());
}
