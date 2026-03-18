#include "registry.h"
#include "example/example_challenge.h"
#include "crypto/crypto.h"
#include "quantum/quantum.h"

namespace challenges {

void init() {
  // Initialize challenge modules (each registers its own CLI commands)
  challenges::example::init();
  challenges::crypto::init();
  challenges::quantum::init();
}

}  // namespace challenges
