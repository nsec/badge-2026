#pragma once

#include <cmath>
#include <cstdint>

#include <badge_config.h>

namespace core::animation {

inline constexpr uint8_t kMaxLights = badge::config::animation::max_lights;
inline constexpr float kFrameInterval = 1.0f / badge::config::animation::target_fps;
inline constexpr uint32_t kFrameIntervalMs = badge::config::animation::frame_interval_ms;

struct Vec2 {
  float x = 0;
  float y = 0;

  constexpr Vec2 operator+(Vec2 other) const {
    return {x + other.x, y + other.y};
  }

  constexpr Vec2 operator-(Vec2 other) const {
    return {x - other.x, y - other.y};
  }

  constexpr Vec2 operator*(float scale) const {
    return {x * scale, y * scale};
  }

  float distanceTo(Vec2 other) const {
    float dx = x - other.x;
    float dy = y - other.y;
    return sqrtf(dx * dx + dy * dy);
  }
};

struct RGBF {
  float r = 0;
  float g = 0;
  float b = 0;

  constexpr RGBF operator+(RGBF other) const {
    return {r + other.r, g + other.g, b + other.b};
  }

  constexpr RGBF operator*(float scale) const {
    return {r * scale, g * scale, b * scale};
  }

  constexpr RGBF clamped() const {
    auto clamp01 = [](float value) {
      return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    };

    return {clamp01(r), clamp01(g), clamp01(b)};
  }

  uint8_t r8() const {
    return correctedOutput(clamped().r, badge::config::animation::white_balance_r);
  }

  uint8_t g8() const {
    return correctedOutput(clamped().g, badge::config::animation::white_balance_g);
  }

  uint8_t b8() const {
    return correctedOutput(clamped().b, badge::config::animation::white_balance_b);
  }

private:
  static uint8_t correctedOutput(float value, float whiteBalance) {
    float corrected = powf(value, badge::config::animation::gamma) * whiteBalance;

    if (corrected > 1.0f) {
      corrected = 1.0f;
    }

    return static_cast<uint8_t>(corrected * 255.0f + 0.5f);
  }
};

/// HSV to RGB conversion (all inputs 0..1).
inline RGBF hsvToRgb(float hue, float saturation, float value) {
  hue = hue - floorf(hue);  // wrap to [0,1)
  float chroma = value * saturation;
  float secondary = chroma * (1.0f - fabsf(fmodf(hue * 6.0f, 2.0f) - 1.0f));
  float match = value - chroma;
  float red, green, blue;
  int region = static_cast<int>(hue * 6.0f);

  switch (region % 6) {
    case 0:
      red = chroma;
      green = secondary;
      blue = 0;
      break;
    case 1:
      red = secondary;
      green = chroma;
      blue = 0;
      break;
    case 2:
      red = 0;
      green = chroma;
      blue = secondary;
      break;
    case 3:
      red = 0;
      green = secondary;
      blue = chroma;
      break;
    case 4:
      red = secondary;
      green = 0;
      blue = chroma;
      break;
    default:
      red = chroma;
      green = 0;
      blue = secondary;
      break;
  }

  return {red + match, green + match, blue + match};
}

// ---------------------------------------------------------------------------
// Oklab perceptual color space
// ---------------------------------------------------------------------------

struct Oklab {
  float L = 0;  // lightness
  float a = 0;  // green-red
  float b = 0;  // blue-yellow

  constexpr Oklab operator+(Oklab other) const {
    return {L + other.L, a + other.a, b + other.b};
  }

  constexpr Oklab operator*(float scale) const {
    return {L * scale, a * scale, b * scale};
  }
};

/// Convert linear RGB (0..1) to Oklab.
inline Oklab rgbToOklab(RGBF color) {
  float lmsLong = 0.4122214708f * color.r + 0.5363325363f * color.g + 0.0514459929f * color.b;
  float lmsMedium = 0.2119034982f * color.r + 0.6806995451f * color.g + 0.1073969566f * color.b;
  float lmsShort = 0.0883024619f * color.r + 0.2024326100f * color.g + 0.7092749381f * color.b;
  float longCbrt = cbrtf(lmsLong);
  float mediumCbrt = cbrtf(lmsMedium);
  float shortCbrt = cbrtf(lmsShort);

  return {0.2104542553f * longCbrt + 0.7936177850f * mediumCbrt - 0.0040720468f * shortCbrt,
          1.9779984951f * longCbrt - 2.4285922050f * mediumCbrt + 0.4505937099f * shortCbrt,
          0.0259040371f * longCbrt + 0.7827717662f * mediumCbrt - 0.8086757660f * shortCbrt};
}

/// Convert Oklab to linear RGB (0..1). May produce out-of-gamut values; clamp after.
inline RGBF oklabToRgb(Oklab color) {
  float longCbrt = color.L + 0.3963377774f * color.a + 0.2158037573f * color.b;
  float mediumCbrt = color.L - 0.1055613458f * color.a - 0.0638541728f * color.b;
  float shortCbrt = color.L - 0.0894841775f * color.a - 1.2914855480f * color.b;
  float lmsLong = longCbrt * longCbrt * longCbrt;
  float lmsMedium = mediumCbrt * mediumCbrt * mediumCbrt;
  float lmsShort = shortCbrt * shortCbrt * shortCbrt;

  return {+4.0767416621f * lmsLong - 3.3077115913f * lmsMedium + 0.2309699292f * lmsShort,
          -1.2684380046f * lmsLong + 2.6097574011f * lmsMedium - 0.3413193965f * lmsShort,
          -0.0041960863f * lmsLong - 0.7034186147f * lmsMedium + 1.7076147010f * lmsShort};
}

// ---------------------------------------------------------------------------
// Temporal dithering
// ---------------------------------------------------------------------------

/// Gamma-correct and dither a single channel. Noise-based temporal dithering
/// spreads quantization error across all frequencies, avoiding the coherent
/// flicker that ordered dithering produces at low PWM values.
inline uint8_t ditheredOutput(float value, float whiteBalance, uint8_t frameCounter, uint8_t led, uint8_t channel) {
  float corrected = powf(value, badge::config::animation::gamma) * whiteBalance * 255.0f;

  if (corrected < 0.0f) {
    corrected = 0.0f;
  }

  if (corrected > 255.0f) {
    corrected = 255.0f;
  }

  uint8_t quantized = static_cast<uint8_t>(corrected);

  if (quantized >= 255) {
    return 255;
  }

  float fractional = corrected - quantized;
  // Noise dither: hash of frame counter, LED index, and channel produces a
  // unique threshold per pixel/channel/frame, converting coherent flicker
  // into perceptually invisible noise. Each channel uses a different frame
  // multiplier so R/G/B follow independent temporal sequences, avoiding
  // correlated hue shifts.
  static constexpr uint8_t kFrameMult[] = {251, 127, 31};
  uint8_t noise = static_cast<uint8_t>(frameCounter * kFrameMult[channel] + led * 173u);
  float threshold = noise * (1.0f / 256.0f);

  return quantized + ((fractional > threshold) ? 1 : 0);
}

/// A single virtual light source (runtime state, evaluated each frame).
struct Light {
  Vec2 pos;
  RGBF color;
  float radius = 0.3f;
  float intensity = 1.0f;
};

}  // namespace core::animation
