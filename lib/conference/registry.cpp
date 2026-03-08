#include "registry.h"
#include "schedule/schedule.h"
#include <Arduino.h>

namespace conference {

void init() {
  Serial.println("Conference modules initialized");
  // Initialize conference modules (each registers its own CLI commands)
  conference::schedule::init();
}

}  // namespace conference
