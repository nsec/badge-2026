#include "registry.h"
#include "quantum/quantum.h"

namespace challenges {

void init() {
  // Initialize challenge modules (each registers its own CLI commands)
  challenges::quantum::init();
}

}  // namespace challenges
