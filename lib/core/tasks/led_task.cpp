#include "tasks/led_task.h"

#include <Arduino.h>

#include "hardware/rgb_led.h"

namespace core {

namespace led {

void solidColor(uint8_t r, uint8_t g, uint8_t b) {
  hw::rgbSetAll(r, g, b);
  delay(1000);
}

void pixelWalk() {
  for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
    hw::rgbClear();
    hw::rgbSetPixel(i, 255, 0, 255);
    hw::rgbShow();
    delay(100);
  }
}

void rainbow() {
  for (int frame = 0; frame < 256; frame += 4) {
    for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
      uint8_t hue = (frame + i * 256 / hw::RGB_LED_COUNT) & 0xFF;
      uint8_t r, g, b;
      uint8_t region = hue / 43;
      uint8_t remainder = (hue - region * 43) * 6;
      switch (region) {
        case 0:  r = 255; g = remainder; b = 0; break;
        case 1:  r = 255 - remainder; g = 255; b = 0; break;
        case 2:  r = 0; g = 255; b = remainder; break;
        case 3:  r = 0; g = 255 - remainder; b = 255; break;
        case 4:  r = remainder; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - remainder; break;
      }
      hw::rgbSetPixel(i, r, g, b);
    }
    hw::rgbShow();
    delay(20);
  }
}

void off() {
  hw::rgbClear();
  delay(500);
}

}  // namespace led

Queue<LedCommand> *g_ledQueue = nullptr;

void LedTask::run() {
  for (;;) {
    LedCommand cmd;
    if (!_queue.receive(cmd)) continue;

    switch (cmd.type) {
      case LedCommandType::SolidRed:   led::solidColor(255, 0, 0); break;
      case LedCommandType::SolidGreen: led::solidColor(0, 255, 0); break;
      case LedCommandType::SolidBlue:  led::solidColor(0, 0, 255); break;
      case LedCommandType::SolidWhite: led::solidColor(255, 255, 255); break;
      case LedCommandType::PixelWalk:  led::pixelWalk(); break;
      case LedCommandType::Rainbow:    led::rainbow(); break;
      case LedCommandType::Off:        led::off(); break;
    }
  }
}

}  // namespace core
