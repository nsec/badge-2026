#pragma once

// Centralized badge pin mapping.

namespace badge {
namespace pins {

// Placeholder: many ESP32-S3 devkits use GPIO2 or GPIO48 for onboard LED.
// Adjust to your schematic.
static constexpr int LED_STATUS = 48;

// RGB LED strip (WS2812 / NeoPixel) — 18 LEDs on IO8
static constexpr int LED_RGB = 8;

// Buttons (active-low, directly from pinout diagram)
static constexpr int BTN_A     = 17;  // IO17
static constexpr int BTN_B     = 18;  // IO18
static constexpr int BTN_LEFT  = 15;  // IO15
static constexpr int BTN_RIGHT = 16;  // IO16
static constexpr int BTN_UP    = 13;  // IO13
static constexpr int BTN_DOWN  = 14;  // IO14

} // namespace pins
} // namespace badge
