#pragma once

// ESP-IDF system API stubs for native simulator

#include <cstdlib>

inline void esp_restart() {
  std::exit(0);
}

inline uint32_t esp_get_free_heap_size() {
  return 256 * 1024;
}
