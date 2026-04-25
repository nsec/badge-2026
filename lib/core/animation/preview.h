#pragma once

// Shared buffer for streaming light source state to the LED preview visualizer.
// Only meaningful on native builds — the ESP32 build never reads this.

#include "animation/types.h"

namespace core::animation {

struct PreviewFrame {
  Light lights[kMaxLights];
  uint8_t lightCount = 0;
};

// Written by AnimationEngine::tick(), read by the NeoPixel stub's show().
extern PreviewFrame g_previewFrame;

}  // namespace core::animation
