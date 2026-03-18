#include "rgb_led.h"
#include "board_pins.h"

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace {

Adafruit_NeoPixel g_strip(core::hw::RGB_LED_COUNT, badge::pins::LED_RGB, NEO_GRB + NEO_KHZ800);

}  // namespace

namespace core {
namespace hw {

void rgbInit() {
  g_strip.begin();
  g_strip.setBrightness(128);  // safe default
  g_strip.clear();
  g_strip.show();
  //Serial.println("RGB LEDs initialized (18 LEDs on IO8)");
}

void rgbSetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index >= RGB_LED_COUNT)
    return;
  g_strip.setPixelColor(index, g_strip.Color(r, g, b));
}

void rgbShow() {
  g_strip.show();
}

void rgbSetAll(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t color = g_strip.Color(r, g, b);
  for (uint8_t i = 0; i < RGB_LED_COUNT; i++) {
    g_strip.setPixelColor(i, color);
  }
  g_strip.show();
}

void rgbClear() {
  g_strip.clear();
  g_strip.show();
}

void rgbSetBrightness(uint8_t brightness) {
  g_strip.setBrightness(brightness);
}

}  // namespace hw
}  // namespace core
