#include "hardware/eink.h"
#include "hardware/board_pins.h"
#include "hardware/nfc.h"
#include "hardware/nsec_logo.h"

#include <Arduino.h>

namespace {

core::hw::EinkDisplay g_display(GxEPD2_154_D67(badge::pins::EINK_CS, badge::pins::EINK_DC, badge::pins::EINK_RST,
                                               badge::pins::EINK_BUSY));

bool g_initialized = false;

}  // namespace

namespace core {
namespace hw {

bool einkInit() {
  if (g_initialized)
    return true;

  // Share the HSPI bus already initialized by nfcInit().
  // GxEPD2's init() accepts an SPIClass& to avoid creating a second bus instance.
  // ESP32 SPI transactions handle per-device bus locking automatically.
  g_display.init(115200, true, 50, false, nfcSPI(), SPISettings(4000000, MSBFIRST, SPI_MODE0));

  // Draw NorthSec logo on boot
  g_display.setRotation(1);
  g_display.setFullWindow();
  g_display.firstPage();
  do {
    g_display.fillScreen(GxEPD_WHITE);
    g_display.drawInvertedBitmap(0, 0, badge::LOGO_BITMAP, badge::LOGO_WIDTH, badge::LOGO_HEIGHT, GxEPD_BLACK);
  } while (g_display.nextPage());

  g_initialized = true;
  Serial.println("E-Ink: GDEH0154D67 200x200 initialized (shared SPI)");
  return true;
}

EinkDisplay &einkDisplay() {
  return g_display;
}

}  // namespace hw
}  // namespace core
