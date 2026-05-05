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

// SPI bus (shared by NFC and E-Ink display)
static constexpr int SPI_SCK = 9;
static constexpr int SPI_MISO = 10;
static constexpr int SPI_MOSI = 11;

// NFC (ST25R3916)
static constexpr int NFC_CS = 12;
static constexpr int NFC_INT = 21;
static constexpr int NFC_LED = 40;

// I2C bus (shared: dock PCI connector, SAO headers, light sensor)
static constexpr int I2C_SDA = 5;
static constexpr int I2C_SCL = 6;

// E-Ink display (GDEH0154D67 200x200, SSD1681)
static constexpr int EINK_CS = 41;
static constexpr int EINK_DC = 42;
static constexpr int EINK_RST = 45;
static constexpr int EINK_BUSY = 46;

}  // namespace pins
}  // namespace badge
