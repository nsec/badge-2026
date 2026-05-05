#pragma once

// Adafruit NeoPixel stub for native simulator.
// Captures pixel data and broadcasts frames via UDP to localhost:9876
// for the LED preview visualizer (tools/led_preview.py).
//
// UDP packet format:
//   Bytes 0..53  : LED RGB data (18 × 3)
//   Byte  54     : light count (0..8)
//   Per light (28 bytes each):
//     float x, y       — position on [0,1] plane
//     float r, g, b    — color (linear, pre-gamma)
//     float radius     — falloff radius
//     float intensity  — current intensity

#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "animation/preview.h"

#define NEO_GRB    0x01
#define NEO_KHZ800 0x02

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(uint16_t n, int16_t pin, uint8_t type = NEO_GRB + NEO_KHZ800) : _numLeds(n) {
    if (n > kMaxLeds)
      n = kMaxLeds;
    std::memset(_pixels, 0, sizeof(_pixels));
  }

  void begin() {
    _sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sock >= 0) {
      std::memset(&_dest, 0, sizeof(_dest));
      _dest.sin_family = AF_INET;
      _dest.sin_port = htons(9876);
      inet_pton(AF_INET, "127.0.0.1", &_dest.sin_addr);
    }
  }

  void show() {
    if (_sock < 0)
      return;

    // Build packet: LED pixels + light source state.
    uint8_t buf[512];
    size_t off = 0;

    // LED RGB data.
    size_t ledBytes = _numLeds * 3;
    std::memcpy(buf + off, _pixels, ledBytes);
    off += ledBytes;

    // Light count.
    const auto &pf = core::animation::g_previewFrame;
    buf[off++] = pf.lightCount;

    // Per-light data (7 floats = 28 bytes each).
    for (uint8_t i = 0; i < pf.lightCount; i++) {
      const auto &l = pf.lights[i];
      float data[7] = {l.pos.x, l.pos.y, l.color.r, l.color.g, l.color.b, l.radius, l.intensity};
      std::memcpy(buf + off, data, sizeof(data));
      off += sizeof(data);
    }

    sendto(_sock, buf, off, 0, reinterpret_cast<struct sockaddr *>(&_dest), sizeof(_dest));
  }

  void clear() {
    std::memset(_pixels, 0, _numLeds * 3);
  }

  void setBrightness(uint8_t b) {
    (void)b;
  }

  void setPixelColor(uint16_t n, uint32_t c) {
    if (n >= _numLeds)
      return;
    _pixels[n * 3 + 0] = (c >> 16) & 0xFF;  // R
    _pixels[n * 3 + 1] = (c >> 8) & 0xFF;   // G
    _pixels[n * 3 + 2] = c & 0xFF;          // B
  }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

private:
  static constexpr uint16_t kMaxLeds = 64;
  uint16_t _numLeds;
  uint8_t _pixels[kMaxLeds * 3]{};
  int _sock = -1;

  struct sockaddr_in _dest {};
};
