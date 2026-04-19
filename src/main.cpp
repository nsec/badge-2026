/*
 * NorthSec Badge 2026 Firmware (ESP32-S3)
 * PlatformIO + Arduino (no .ino)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include <cstring>

#include <badge_config.h>
#include <core.h>
#include <animation/parser.h>
#include <animation/storage.h>
#include <hardware/line_buffered_stream.h>
#include <hardware/light_sensor.h>
#include <hardware/serial_mutex.h>

// Conditionally include conference or challenges based on build flags
#ifdef HAS_CONFERENCE
  #include <../lib/conference/registry.h>
#endif

#ifdef HAS_CHALLENGES
  #include <../lib/challenges/registry.h>

// Simple obfuscation of a cleartext string to get past the CICD check.
const std::string cleartextFirmwareFlg =
    "\x46\x4c\x41\x47\x2d\x69\x2d\x63\x34\x6e\x2d\x68\x34\x7a\x2d\x66\x31\x72\x6d\x77\x34\x72\x33";
#endif

void setup() {
  Serial.setTxBufferSize(4096);
#ifndef NATIVE_BUILD
  Serial.setRxBufferSize(4096);
#endif
  Serial.begin(115200);

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
#ifdef CONFERENCE_ONLY
  Serial.println("Mode:  conference-only");
#elif defined(HAS_CHALLENGES)
  Serial.println("Mode:  ctf");
#elif defined(HAS_CONFERENCE)
  Serial.println("Mode:  conference (dual)");
#endif
  Serial.println("=========================");
  Serial.flush();

  core::hw::statusLedInit();
  core::hw::rgbInit();
  core::hw::buttonsInit();
  // Serial.println("LEDs initialized");

  core::storage::socialNvsInit();
  core::storage::configNvsInit();

  if (core::animation::storageInit()) {
    Serial.println("Animation filesystem initialized");
  } else {
    Serial.println("WARNING: Animation filesystem init failed");
  }

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

  // Light sensor shares the same I2C pins as the dock. Each read temporarily
  // borrows the bus (Wire1 master) then restores the dock slave (Wire).
  if (core::hw::lightSensorInit()) {
    Serial.println("Light sensor initialized");
  } else {
    Serial.println("Light sensor init failed");
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

  // Apply saved brightness
  {
    auto cfg = core::storage::configRead();
    core::hw::rgbSetBrightness(cfg.brightness);
  }

  // Create queues
  static core::Queue<core::ControllerEvent> controllerQueue(badge::config::queues::controller_depth);
  static core::Queue<core::LedCommand> ledQueue(badge::config::queues::led_depth);
  static core::Queue<core::CliResponse> cliQueue(badge::config::queues::cli_depth);
  static core::Queue<core::NfcCommand> nfcQueue(badge::config::queues::nfc_depth);
  static core::Queue<core::DisplayCommand> displayQueue(badge::config::queues::display_depth);
  static core::Queue<core::PortalCommand> portalQueue(badge::config::queues::portal_depth);

  core::g_controllerQueue = &controllerQueue;
  core::g_ledQueue = &ledQueue;
  core::g_cliQueue = &cliQueue;
  core::g_nfcQueue = &nfcQueue;
  core::g_displayQueue = &displayQueue;

  // Create tasks
  static core::LedTask ledTask(ledQueue);
  static core::ControllerTask controllerTask(controllerQueue, ledQueue, cliQueue, nfcQueue, displayQueue, portalQueue);
  static core::CliTask cliTask;
  static core::ButtonTask buttonTask(controllerQueue);
  static core::NfcTask nfcTask(nfcQueue, ledQueue, displayQueue);
  static core::DockTask dockTask(dockEventQueue, ledQueue);
  static core::DisplayTask displayTask(displayQueue);
  static core::LightTask lightTask;
  static core::PortalTask portalTask(portalQueue, controllerQueue);

  core::heartbeatStart();

  ledTask.start();
  controllerTask.start();
  cliTask.start();
  buttonTask.start();
  nfcTask.start();
  dockTask.start();
  displayTask.start();
  portalTask.start();

  // Show boot logo via DisplayTask (moved from einkInit)
  {
    core::DisplayCommand dc{};
    dc.type = core::DisplayCommand::Type::ShowLogo;
    displayQueue.send(dc, core::Milliseconds(0));
  }

  // Play boot LED animation
  {
    auto result = core::animation::loadAndParseAnimation("boot");
    if (result.ok) {
      core::LedCommand lc(core::LedCommandType::Animation);
      lc.animation = result.def.release();
      ledQueue.send(lc, core::Milliseconds(0));
    }
  }

  Serial.println("Setup complete!");
  Serial.println("Type 'help' for commands.");
  Serial.flush();
  lightTask.start();
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
