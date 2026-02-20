#include "registry.h"
#include "example_challenge.h"
#include "crypto.h"

namespace challenges {

void init() {
  // Register/init challenge modules here.
  challenges::example::init();
}

void tick() {
  // Periodic hook for challenges.
  //challenges::example::tick();
}

} // namespace challenges
