#pragma once

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>

namespace core {
namespace hw {

/// Display type alias: GDEH0154D67 200x200 BW, SSD1681
using EinkDisplay = GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>;

/// Initialize the e-ink display. Must be called after nfcInit() (shares SPI bus).
bool einkInit();

/// Get the display instance for drawing operations.
EinkDisplay &einkDisplay();

}  // namespace hw
}  // namespace core
