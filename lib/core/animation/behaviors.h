#pragma once

#include <cmath>

#include "animation/palette.h"
#include "animation/types.h"

namespace core::animation {

// ---------------------------------------------------------------------------
// Motion behaviors — evaluate to Vec2 (position on the badge plane)
// ---------------------------------------------------------------------------

struct StaticMotion {
  Vec2 pos;
};

struct OrbitMotion {
  Vec2 center;
  float rx;      // x radius
  float ry;      // y radius
  float period;  // seconds per revolution
  float phase;   // starting angle in radians
  bool reverse;  // traverse clockwise instead of counter-clockwise
};

struct PingPongMotion {
  Vec2 a;
  Vec2 b;
  float period;  // seconds for a full round trip
};

/// Pseudo-random drift using summed sine waves at irrational frequency ratios.
/// Looks organic and non-repeating over long periods, no noise table needed.
struct NoiseDriftMotion {
  Vec2 center;
  float amplitude;  // max displacement from center
  float speed;      // scales all frequencies (1.0 = normal)
};

/// Lissajous figure: x = rx*sin(a*t*speed + phase), y = ry*sin(b*t*speed).
/// Produces figure-8s, loops, and complex closed curves depending on a/b ratio.
/// `speed` scales time uniformly so you can adjust traversal rate without
/// changing the figure shape (which is determined by the freqA/freqB ratio).
struct LissajousMotion {
  Vec2 center;
  float rx;     // x amplitude
  float ry;     // y amplitude
  float freqA;  // x frequency (Hz)
  float freqB;  // y frequency (Hz)
  float phase;  // x phase offset (radians)
  float speed;  // time multiplier (1.0 = normal)
};

/// Catmull-Rom spline through up to 8 control points, looping.
struct SplineMotion {
  static constexpr uint8_t kMaxPoints = 8;

  Vec2 points[kMaxPoints];
  uint8_t count;  // number of control points (>= 2)
  float period;   // seconds for one complete loop
  bool reverse;   // traverse control points in reverse order
};

enum class MotionType : uint8_t { Static, Orbit, PingPong, NoiseDrift, Lissajous, Spline };

struct Motion {
  MotionType type;

  union {
    StaticMotion static_;
    OrbitMotion orbit;
    PingPongMotion pingpong;
    NoiseDriftMotion noiseDrift;
    LissajousMotion lissajous;
    SplineMotion spline;
  };

  constexpr Motion() : type(MotionType::Static), static_{{0.5f, 0.5f}} {}

  constexpr Motion(StaticMotion m) : type(MotionType::Static), static_(m) {}

  constexpr Motion(OrbitMotion m) : type(MotionType::Orbit), orbit(m) {}

  constexpr Motion(PingPongMotion m) : type(MotionType::PingPong), pingpong(m) {}

  constexpr Motion(NoiseDriftMotion m) : type(MotionType::NoiseDrift), noiseDrift(m) {}

  constexpr Motion(LissajousMotion m) : type(MotionType::Lissajous), lissajous(m) {}

  constexpr Motion(SplineMotion m) : type(MotionType::Spline), spline(m) {}
};

/// Catmull-Rom interpolation between four points at parameter t in [0,1].
/// See wikipedia entry :|
inline Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;

  return {0.5f * (2.0f * p1.x + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
          0.5f * (2.0f * p1.y + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)};
}

inline Vec2 evaluate(const Motion &motion, float time) {
  switch (motion.type) {
    case MotionType::Static:
      return motion.static_.pos;

    case MotionType::Orbit: {
      const auto &orbit = motion.orbit;

      float direction = orbit.reverse ? -1.0f : 1.0f;
      float angle = orbit.phase + direction * (2.0f * 3.14159265f * time) / orbit.period;

      return {orbit.center.x + orbit.rx * cosf(angle), orbit.center.y + orbit.ry * sinf(angle)};
    }

    case MotionType::PingPong: {
      const auto &pingPong = motion.pingpong;

      // Triangle wave: 0 -> 1 -> 0 over `period` seconds
      float phase = fmodf(time, pingPong.period) / pingPong.period;
      float lerp = 1.0f - fabsf(2.0f * phase - 1.0f);

      return {pingPong.a.x + (pingPong.b.x - pingPong.a.x) * lerp, pingPong.a.y + (pingPong.b.y - pingPong.a.y) * lerp};
    }

    case MotionType::NoiseDrift: {
      const auto &drift = motion.noiseDrift;

      float speed = drift.speed;
      float driftX = drift.amplitude *
                     (sinf(time * speed * 1.0f) + 0.5f * sinf(time * speed * 1.7f + 0.3f) +
                      0.3f * sinf(time * speed * 2.9f + 1.1f)) /
                     1.8f;  // normalize: max sum = 1 + 0.5 + 0.3 = 1.8
      float driftY = drift.amplitude *
                     (sinf(time * speed * 1.3f) + 0.5f * sinf(time * speed * 2.1f + 0.7f) +
                      0.3f * sinf(time * speed * 3.7f + 2.3f)) /
                     1.8f;

      return {drift.center.x + driftX, drift.center.y + driftY};
    }

    case MotionType::Lissajous: {
      const auto &lissajous = motion.lissajous;

      constexpr float twoPi = 2.0f * 3.14159265f;
      float scaledTime = time * lissajous.speed;
      float posX = lissajous.center.x + lissajous.rx * sinf(twoPi * lissajous.freqA * scaledTime + lissajous.phase);
      float posY = lissajous.center.y + lissajous.ry * sinf(twoPi * lissajous.freqB * scaledTime);

      return {posX, posY};
    }

    case MotionType::Spline: {
      const auto &spline = motion.spline;

      if (spline.count < 2) {
        return spline.points[0];
      }

      float phase = fmodf(time, spline.period) / spline.period;

      if (spline.reverse) {
        phase = 1.0f - phase;
      }

      const float indexFloat = phase * spline.count;
      int segment = static_cast<int>(indexFloat);

      if (segment >= spline.count) {
        segment = spline.count - 1;
      }

      const float fractional = indexFloat - segment;

      // Looping Catmull-Rom: wrap indices
      const int i0 = (segment - 1 + spline.count) % spline.count;
      const int i1 = segment % spline.count;
      const int i2 = (segment + 1) % spline.count;
      const int i3 = (segment + 2) % spline.count;

      return catmullRom(spline.points[i0], spline.points[i1], spline.points[i2], spline.points[i3], fractional);
    }
  }

  return {0.5f, 0.5f};
}

// ---------------------------------------------------------------------------
// Color behaviors — evaluate to RGBF
// ---------------------------------------------------------------------------

struct StaticColor {
  RGBF color;
};

struct HueCycleColor {
  float fromHue;  // 0..1 (0 = red, 0.33 = green, 0.67 = blue)
  float toHue;    // 0..1
  float period;   // seconds for one full sweep
  bool pingPong;  // bounce between fromHue and toHue instead of wrapping
};

struct PaletteColor {
  static constexpr uint8_t kMaxColors = 4;
  RGBF colors[kMaxColors];
  uint8_t count;  // how many entries are used (2..kMaxColors)
  float period;   // seconds to cycle through the entire palette
};

struct PaletteSlotColor {
  PaletteSlot slot;
};

enum class ColorType : uint8_t { Static, HueCycle, Palette, PaletteSlot };

struct Color {
  ColorType type;

  union {
    StaticColor static_;
    HueCycleColor hueCycle;
    PaletteColor palette;
    PaletteSlotColor paletteSlot;
  };

  constexpr Color() : type(ColorType::Static), static_{{1, 1, 1}} {}

  constexpr Color(StaticColor c) : type(ColorType::Static), static_(c) {}

  constexpr Color(HueCycleColor c) : type(ColorType::HueCycle), hueCycle(c) {}

  constexpr Color(PaletteColor c) : type(ColorType::Palette), palette(c) {}

  constexpr Color(PaletteSlotColor c) : type(ColorType::PaletteSlot), paletteSlot(c) {}
};

inline RGBF evaluate(const Color &color, float time, const ResolvedPalette &palette) {
  switch (color.type) {
    case ColorType::Static:
      return color.static_.color;

    case ColorType::HueCycle: {
      const auto &hueCycle = color.hueCycle;
      float phase = fmodf(time, hueCycle.period) / hueCycle.period;

      if (hueCycle.pingPong) {
        phase = 1.0f - fabsf(2.0f * phase - 1.0f);
      }

      float hue = hueCycle.fromHue + (hueCycle.toHue - hueCycle.fromHue) * phase;
      return hsvToRgb(hue, 0.9f, 1.0f);
    }

    case ColorType::Palette: {
      const auto &pal = color.palette;

      if (pal.count < 2) {
        return pal.colors[0];
      }

      const float phase = fmodf(time, pal.period) / pal.period;
      const float indexFloat = phase * (pal.count - 1);
      const int lowerIndex = static_cast<int>(indexFloat);
      int upperIndex = lowerIndex + 1;

      if (upperIndex >= pal.count) {
        upperIndex = 0;  // wrap
      }

      const float fractional = indexFloat - lowerIndex;
      const RGBF &colorA = pal.colors[lowerIndex];
      const RGBF &colorB = pal.colors[upperIndex];

      return {colorA.r + (colorB.r - colorA.r) * fractional, colorA.g + (colorB.g - colorA.g) * fractional,
              colorA.b + (colorB.b - colorA.b) * fractional};
    }

    case ColorType::PaletteSlot:
      return palette[color.paletteSlot.slot];
  }

  return {1, 1, 1};
}

// ---------------------------------------------------------------------------
// Scalar behaviors — evaluate to float (used for radius, intensity)
// ---------------------------------------------------------------------------

struct StaticScalar {
  float value;
};

struct BreatheScalar {
  float base;       // center value
  float amplitude;  // swing (value oscillates base +/- amplitude)
  float period;     // seconds per cycle
  float phase;      // starting phase in radians
};

/// Smooth pseudo-random variation using summed sines, less metronomic than Breathe.
struct NoiseScalar {
  float base;       // center value
  float amplitude;  // max swing from base
  float speed;      // frequency scale (1.0 = normal)
};

enum class ScalarType : uint8_t { Static, Breathe, Noise };

struct Scalar {
  ScalarType type;

  union {
    StaticScalar static_;
    BreatheScalar breathe;
    NoiseScalar noise;
  };

  constexpr Scalar() : type(ScalarType::Static), static_{1.0f} {}

  constexpr Scalar(float v) : type(ScalarType::Static), static_{v} {}

  constexpr Scalar(StaticScalar s) : type(ScalarType::Static), static_(s) {}

  constexpr Scalar(BreatheScalar b) : type(ScalarType::Breathe), breathe(b) {}

  constexpr Scalar(NoiseScalar n) : type(ScalarType::Noise), noise(n) {}
};

inline float evaluate(const Scalar &scalar, float time) {
  switch (scalar.type) {
    case ScalarType::Static:
      return scalar.static_.value;

    case ScalarType::Breathe: {
      const auto &breathe = scalar.breathe;
      const float angle = breathe.phase + (2.0f * 3.14159265f * time) / breathe.period;

      return breathe.base + breathe.amplitude * sinf(angle);
    }

    case ScalarType::Noise: {
      const auto &noise = scalar.noise;
      const float noiseValue = (sinf(time * noise.speed * 1.0f) + 0.5f * sinf(time * noise.speed * 1.7f + 0.3f) +
                                0.3f * sinf(time * noise.speed * 2.9f + 1.1f)) /
                               1.8f;  // normalized to [-1, 1]

      return noise.base + noise.amplitude * noiseValue;
    }
  }

  return 1.0f;
}

}  // namespace core::animation
