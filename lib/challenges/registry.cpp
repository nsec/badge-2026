#include "registry.h"
#include "example_challenge.h"
#include "ics/ics_challenge.h"
#include "crypto.h"

namespace challenges {

void init() {
  // Register/init challenge modules here.
  challenges::example::init();
  challenges::crypto::init();
  challenges::ics::init();
}

void tick() {
  // Periodic hook for challenges.
  challenges::example::tick();
  challenges::ics::tick();
}

} // namespace challenges
