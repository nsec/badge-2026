#pragma once

#include <Arduino.h>

namespace core {
namespace hw {

// Number of RGB LEDs on the badge
static constexpr uint8_t RGB_LED_COUNT = 18;

/**
 * Initialize the RGB LED strip (WS2812/NeoPixel on IO8).
 */
void rgbInit();

/**
 * Set a single LED to the given color.
 * Call rgbShow() after setting all desired LEDs.
 */
void rgbSetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Set all LEDs to the same color.
 * Calls rgbShow() automatically.
 */
void rgbSetAll(uint8_t r, uint8_t g, uint8_t b);

/**
 * Push the pixel buffer to the LED strip.
 */
void rgbShow();

/**
 * Turn off all LEDs.
 */
void rgbClear();

/**
 * Set overall brightness (0-255). Default is 32.
 */
void rgbSetBrightness(uint8_t brightness);

} // namespace hw
} // namespace core
