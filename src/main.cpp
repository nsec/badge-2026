/*
 * NorthSec Badge 2026 Firmware (ESP32-S3)
 * PlatformIO + Arduino (no .ino)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include <core.h>

// Conditionally include conference or challenges based on build flags
#ifdef HAS_CONFERENCE
  #include <../lib/conference/registry.h>
#endif

#ifdef HAS_CHALLENGES
  #include <../lib/challenges/registry.h>
#endif

void setup() {
  // USB CDC serial - wait for connection or timeout after 5 seconds
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 5000) {
    delay(100);
  }
  delay(500);  // Extra delay for stability
  
  // Send test pattern
  for(int i = 0; i < 10; i++) {
    Serial.println();
  }
  
  Serial.println("=========================");
  Serial.println("NorthSec Badge 2026");
  Serial.println("BOOT SUCCESSFUL!");
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);
  Serial.println("=========================");
  Serial.flush();

  core::hw::statusLedInit();
  core::hw::rgbInit();
  core::hw::buttonsInit();
  Serial.println("LEDs initialized");

  core::ota::printBootInfo(Serial);

  core::cli::init(Serial);

#ifdef HAS_CONFERENCE
  conference::init();
  Serial.println("Conference modules initialized");
#endif

#ifdef HAS_CHALLENGES
  challenges::init();
  Serial.println("Challenges initialized");
#endif

  Serial.println("Setup complete!");
  Serial.println("Type 'help' for commands.");
  Serial.flush();
}

void loop() {
  core::cli::poll();
  
#ifdef HAS_CONFERENCE
  conference::tick();
#endif

#ifdef HAS_CHALLENGES
  challenges::tick();
#endif

  // Placeholder heartbeat
  static uint32_t last = 0;
  static bool on = false;
  if (millis() - last > 500) {
    last = millis();
    on = !on;
    core::hw::statusLedSet(on);
  }
}
