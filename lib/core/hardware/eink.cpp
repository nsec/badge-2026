#include "hardware/eink.h"
#include "hardware/board_pins.h"
#include "hardware/nfc.h"

#include <Arduino.h>

namespace {

core::hw::EinkDisplay g_display(GxEPD2_154_D67(badge::pins::EINK_CS, badge::pins::EINK_DC, badge::pins::EINK_RST,
                                               badge::pins::EINK_BUSY));

bool g_initialized = false;
bool g_available = false;

/// Detect whether the SSD1681 display controller is present.
/// After g_display.init() the controller is idle (BUSY=LOW), indistinguishable from
/// "no display" via pulldown. So we send a soft-reset (0x12) via SPI and check whether
/// BUSY goes HIGH in response — only a real SSD1681 drives that pin.
bool detectDisplay() {
  pinMode(badge::pins::EINK_BUSY, INPUT_PULLDOWN);

  // Send SSD1681 soft-reset command (0x12) via SPI
  SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE0);
  core::hw::nfcSPI().beginTransaction(spiSettings);
  digitalWrite(badge::pins::EINK_CS, LOW);
  digitalWrite(badge::pins::EINK_DC, LOW);  // command mode
  core::hw::nfcSPI().transfer(0x12);        // SW_RESET
  digitalWrite(badge::pins::EINK_CS, HIGH);
  core::hw::nfcSPI().endTransaction();

  // If display is present, BUSY goes HIGH within ~1ms after soft reset
  unsigned long start = millis();
  while (millis() - start < 100) {
    if (digitalRead(badge::pins::EINK_BUSY) == HIGH) {
      // Wait for reset to complete (BUSY returns LOW)
      while (digitalRead(badge::pins::EINK_BUSY) == HIGH && millis() - start < 500) {
        delay(1);
      }
      return true;
    }
    delay(1);
  }

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
    // Serial.println("E-Ink: no display detected (BUSY pin inactive) — display features disabled");
    return false;
  }

  // Serial.println("E-Ink: GDEH0154D67 200x200 initialized (shared SPI)");
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
