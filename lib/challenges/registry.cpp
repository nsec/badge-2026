#include "registry.h"
#include "quantum/quantum.h"
#include "serialmystery/serialmystery.h"

namespace challenges {

void init() {
  // Initialize challenge modules (each registers its own CLI commands)
  challenges::quantum::init();
  challenges::serialmystery::init();
}

}  // namespace challenges
