#pragma once

// Centralized badge pin mapping.

namespace badge {
namespace pins {

static constexpr int LED_STATUS = 7;

// RGB LED strip (WS2812 / NeoPixel) — 18 LEDs on IO8
static constexpr int LED_RGB = 8;

// Buttons (active-low, directly from pinout diagram)
static constexpr int BTN_A = 17;      // IO17
static constexpr int BTN_B = 18;      // IO18
static constexpr int BTN_LEFT = 15;   // IO15
static constexpr int BTN_RIGHT = 16;  // IO16
static constexpr int BTN_UP = 13;     // IO13
static constexpr int BTN_DOWN = 14;   // IO14

// NFC (ST25R3916) — dedicated SPI bus
static constexpr int NFC_SCK = 9;
static constexpr int NFC_MISO = 10;
static constexpr int NFC_MOSI = 11;
static constexpr int NFC_CS = 12;
static constexpr int NFC_INT = 21;
static constexpr int NFC_LED = 40;

}  // namespace pins
}  // namespace badge
