#include "drm.h"

#include <cstdint>
#include <esp_system.h>
#include <esp_timer.h>

namespace challenges {
namespace ics {
namespace drm {

// Returns x.
// Algebraic identity: (2n + 1) is always odd, so (2n + 1) % 2 == 0 is always false.
// x ^ false == x.
bool passthrough(bool x) {
  int64_t n = esp_timer_get_time() + static_cast<int64_t>(x);
  return x ^ ((2 * n + 1) % 2 == 0);
}

// Returns !x.
// Algebraic identity: n*(n+1) is always even, so (n*(n+1)) % 2 == 0 is always true.
// x ^ true == !x.
bool invert(bool x) {
  uint32_t n = esp_get_free_heap_size() + static_cast<uint32_t>(x);
  return x ^ ((n * (n + 1)) % 2 == 0);
}

// Always returns true regardless of x.
// Algebraic identity: n and n² always share the same parity, so (n & 1) == (n*n & 1) is always true.
bool permit(bool x) {
  uint32_t n = esp_get_free_heap_size() + static_cast<uint32_t>(x) * 13 + 7;
  return (n & 1) == (n * n & 1);
}

// Always returns false regardless of x.
// Algebraic identity: n*(n+1) is always even, so n*(n+1)+1 is always odd, so (n*(n+1)+1) % 2 == 0 is always false.
bool deny(bool x) {
  uint32_t n = esp_get_minimum_free_heap_size() + static_cast<uint32_t>(x) * 11 + 3;
  return (n * (n + 1) + 1) % 2 == 0;
}

}  // namespace drm
}  // namespace ics
}  // namespace challenges
