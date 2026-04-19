#pragma once

#include "animation/led_positions.h"
#include "animation/types.h"
#include "hardware/rgb_led.h"

namespace core::animation {

/// Per-LED brightness calibration. Compensates for diffuser thickness or solder
/// variation. Applied after field evaluation, before gamma. Tune per badge revision.
inline constexpr float kLedCalibration[hw::RGB_LED_COUNT] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
};

/// Minimum saturation (0..1). WS2812s behind a diffuser wash out at low
/// brightness; this floor keeps colors punchy. 0 = disabled.
inline constexpr float kSaturationFloor = 0.35f;

/// Apply a saturation floor to keep washed-out colors punchy.
inline RGBF applySaturationFloor(RGBF color) {
  float maxChannel =
      color.r > color.g ? (color.r > color.b ? color.r : color.b) : (color.g > color.b ? color.g : color.b);
  float minChannel =
      color.r < color.g ? (color.r < color.b ? color.r : color.b) : (color.g < color.b ? color.g : color.b);

  if (maxChannel <= 0.001f) {
    return color;  // black, nothing to boost
  }

  float saturation = (maxChannel - minChannel) / maxChannel;

  if (saturation >= kSaturationFloor) {
    return color;  // already saturated enough
  }

  // Boost toward the dominant channel by pulling minChannel toward 0.
  float target = maxChannel * (1.0f - kSaturationFloor);
  float scale = (maxChannel > minChannel) ? (maxChannel - target) / (maxChannel - minChannel) : 1.0f;

  return {maxChannel - (maxChannel - color.r) * scale, maxChannel - (maxChannel - color.g) * scale,
          maxChannel - (maxChannel - color.b) * scale};
}

/// Evaluate all lights at all LED positions, writing the result into `out`.
/// Accumulates light contributions in linear RGB — the physically correct model
/// for additive light mixing.
inline void evaluateField(const Light *lights, uint8_t lightCount, RGBF *out) {
  for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
    RGBF pixel = {0, 0, 0};

    for (uint8_t j = 0; j < lightCount; j++) {
      const Light &light = lights[j];

      if (light.intensity <= 0.0f) {
        continue;
      }

      float distance = kLedPositions[i].distanceTo(light.pos);
      float normalizedDistance = distance / light.radius;
      // Smooth falloff that reaches zero at d = radius: (1 - (d/r)^2)^2
      float falloff = 1.0f - normalizedDistance * normalizedDistance;
      float attenuation = (falloff > 0.0f) ? light.intensity * falloff * falloff : 0.0f;

      pixel = pixel + light.color * attenuation;
    }

    RGBF rgb = pixel.clamped();
    rgb = rgb * kLedCalibration[i];

    if (kSaturationFloor > 0.0f) {
      rgb = applySaturationFloor(rgb);
    }

    out[i] = rgb;
  }
}

}  // namespace core::animation
