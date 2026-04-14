/*
 * NorthSec Badge 2026 Firmware (ESP32-S3)
 * PlatformIO + Arduino (no .ino)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include <badge_config.h>
#include <core.h>
#include <hardware/line_buffered_stream.h>
#include <hardware/serial_mutex.h>

// Conditionally include conference or challenges based on build flags
#ifdef HAS_CONFERENCE
  #include <../lib/conference/registry.h>
#endif

#ifdef HAS_CHALLENGES
  #include <../lib/challenges/registry.h>
#endif

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);

  unsigned long start = millis();
  while (!Serial && (millis() - start) < 5000) {
    delay(100);
  }
  delay(500);  // Extra delay for stability

  // Send test pattern
  for (int i = 0; i < 10; i++) {
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
  // Serial.println("LEDs initialized");

  core::storage::socialNvsInit();

  if (core::hw::nfcInit()) {
    Serial.println("NFC initialized");
  } else {
    Serial.println("NFC init failed - NFC features disabled");
  }

  // Create dock event queue BEFORE dockInit so the I2C ISR has
  // a valid queue handle from the moment callbacks are registered.
  static core::Queue<core::DockEvent> dockEventQueue(badge::config::queues::dock_depth);
  core::hw::dockSetEventQueue(dockEventQueue.handle());

  core::hw::dockInit();

  if (core::hw::einkInit()) {
    Serial.println("E-Ink initialized");
  } else {
    Serial.println("E-Ink init failed - display features disabled");
  }

  core::ota::printBootInfo(Serial);

  static core::hw::LineBufferedStream bufferedSerial(Serial);
  core::hw::setSafeSerial(&bufferedSerial);
  core::cli::init(bufferedSerial);

#ifdef HAS_CONFERENCE
  conference::init();
  Serial.println("Conference modules initialized");
#endif

#ifdef HAS_CHALLENGES
  challenges::init();
  Serial.println("Challenges initialized");
#endif

  // Create queues
  static core::Queue<core::ControllerEvent> controllerQueue(badge::config::queues::controller_depth);
  static core::Queue<core::LedCommand> ledQueue(badge::config::queues::led_depth);
  static core::Queue<core::CliResponse> cliQueue(badge::config::queues::cli_depth);
  static core::Queue<core::NfcCommand> nfcQueue(badge::config::queues::nfc_depth);
  static core::Queue<core::DisplayCommand> displayQueue(badge::config::queues::display_depth);

  core::g_controllerQueue = &controllerQueue;
  core::g_ledQueue = &ledQueue;
  core::g_cliQueue = &cliQueue;
  core::g_nfcQueue = &nfcQueue;
  core::g_displayQueue = &displayQueue;

  // Create tasks
  static core::LedTask ledTask(ledQueue);
  static core::ControllerTask controllerTask(controllerQueue, ledQueue, cliQueue, nfcQueue, displayQueue);
  static core::CliTask cliTask;
  static core::ButtonTask buttonTask(controllerQueue);
  static core::NfcTask nfcTask(nfcQueue, ledQueue, displayQueue);
  static core::DockTask dockTask(dockEventQueue, ledQueue);
  static core::DisplayTask displayTask(displayQueue);

  core::heartbeatStart();

  ledTask.start();
  controllerTask.start();
  cliTask.start();
  buttonTask.start();
  nfcTask.start();
  dockTask.start();
  displayTask.start();

  // Show boot logo via DisplayTask (moved from einkInit)
  {
    core::DisplayCommand dc{};
    dc.type = core::DisplayCommand::Type::ShowLogo;
    displayQueue.send(dc, core::Milliseconds(0));
  }

  Serial.println("Setup complete!");
  Serial.println("Type 'help' for commands.");
  Serial.flush();
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
