#include "registry.h"
#include "example/example_challenge.h"
#include "crypto/crypto.h"

namespace challenges {

void init() {
  // Register/init challenge modules here.
  challenges::example::init();
  challenges::crypto::init();
}

void tick() {
  // Periodic hook for challenges.
  // challenges::example::tick();
}

}  // namespace challenges
