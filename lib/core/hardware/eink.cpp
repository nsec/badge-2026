#include "hardware/eink.h"
#include "hardware/board_pins.h"
#include "hardware/nfc.h"
#include "hardware/nsec_logo.h"

#include <Arduino.h>

namespace {

core::hw::EinkDisplay g_display(GxEPD2_154_D67(badge::pins::EINK_CS, badge::pins::EINK_DC, badge::pins::EINK_RST,
                                               badge::pins::EINK_BUSY));

bool g_initialized = false;
bool g_available = false;

/// Detect whether the SSD1681 display controller is present by checking the BUSY pin.
/// After hardware reset (performed by g_display.init()), the SSD1681 drives BUSY HIGH
/// briefly while it initializes. With INPUT_PULLDOWN and no display attached, the pin
/// stays LOW.
bool detectDisplay() {
  pinMode(badge::pins::EINK_BUSY, INPUT_PULLDOWN);

  // After init(), the SSD1681 should have completed its power-on sequence.
  // If the controller is present, BUSY will be LOW (idle) but will have been
  // driven HIGH during reset. We can verify presence by triggering a soft reset
  // command and watching for BUSY to go HIGH.

  // Quick check: if BUSY is already HIGH right after init, the display is present
  // (it may still be finishing initialization).
  if (digitalRead(badge::pins::EINK_BUSY) == HIGH) {
    return true;
  }

  // Wait briefly to see if BUSY goes HIGH during post-init activity.
  // The SSD1681 typically asserts BUSY for ~40ms after reset.
  unsigned long start = millis();
  while (millis() - start < 100) {
    if (digitalRead(badge::pins::EINK_BUSY) == HIGH) {
      return true;
    }
    delay(1);
  }

  // BUSY never went HIGH with pulldown enabled — no display connected.
  return false;
}

}  // namespace

namespace core {
namespace hw {

bool einkInit() {
  if (g_initialized)
    return g_available;

  // Share the HSPI bus already initialized by nfcInit().
  // GxEPD2's init() accepts an SPIClass& to avoid creating a second bus instance.
  // ESP32 SPI transactions handle per-device bus locking automatically.
  g_display.init(115200, true, 50, false, nfcSPI(), SPISettings(4000000, MSBFIRST, SPI_MODE0));

  g_initialized = true;

  // Detect whether a physical display is connected via the BUSY pin.
  g_available = detectDisplay();
  if (!g_available) {
    Serial.println("E-Ink: no display detected (BUSY pin inactive) — display features disabled");
    return false;
  }

  // Draw NorthSec logo on boot
  g_display.setRotation(1);
  g_display.setFullWindow();
  g_display.firstPage();
  do {
    g_display.fillScreen(GxEPD_WHITE);
    g_display.drawInvertedBitmap(0, 0, badge::LOGO_BITMAP, badge::LOGO_WIDTH, badge::LOGO_HEIGHT, GxEPD_BLACK);
  } while (g_display.nextPage());

  Serial.println("E-Ink: GDEH0154D67 200x200 initialized (shared SPI)");
  return true;
}

bool einkAvailable() {
  return g_available;
}

EinkDisplay &einkDisplay() {
  return g_display;
}

}  // namespace hw
}  // namespace core
