#pragma once

// Adafruit NeoPixel stub for native simulator

#include <cstdint>

#define NEO_GRB    0x01
#define NEO_KHZ800 0x02

class Adafruit_NeoPixel {
public:
    Adafruit_NeoPixel(uint16_t n, int16_t pin, uint8_t type = NEO_GRB + NEO_KHZ800)
        : _numLeds(n) {}

    void begin() {}
    void show() {}
    void clear() {}
    void setBrightness(uint8_t b) {}

    void setPixelColor(uint16_t n, uint32_t c) {}

    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

private:
    uint16_t _numLeds;
};
