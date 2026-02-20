#pragma once

// Centralized badge pin mapping.

namespace badge {
namespace pins {

// Placeholder: many ESP32-S3 devkits use GPIO2 or GPIO48 for onboard LED.
// Adjust to your schematic.
static constexpr int LED_STATUS = 48;

// RGB LED strip (WS2812 / NeoPixel) — 18 LEDs on IO8
static constexpr int LED_RGB = 8;

} // namespace pins
} // namespace badge
