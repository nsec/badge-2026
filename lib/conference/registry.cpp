#include "registry.h"
#include "schedule.h"
#include <Arduino.h>

namespace conference {

void init() {
    Serial.println("Conference modules initialized");
    schedule::init();
}

void tick() {
    // Called periodically from main loop
    // Update conference features here
}

} // namespace conference
