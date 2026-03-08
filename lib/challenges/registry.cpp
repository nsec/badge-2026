#include "registry.h"
#include "example/example_challenge.h"
#include "crypto/crypto.h"

namespace challenges {

void init() {
  // Initialize challenge modules (each registers its own CLI commands)
  challenges::example::init();
  challenges::crypto::init();
}

}  // namespace challenges
