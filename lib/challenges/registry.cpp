#include "registry.h"
#include "quantum/quantum.h"
#include "serialmystery/serialmystery.h"
#include "ics/ics_challenge.h"

namespace challenges {

void init() {
  // Initialize challenge modules (each registers its own CLI commands)
  challenges::quantum::init();
  challenges::serialmystery::init();
  challenges::ics::init();
}

void tick() {
  // Periodic hook for challenges.
  challenges::ics::tick();
}

}  // namespace challenges
