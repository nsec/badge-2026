#pragma once

#include <cmath>
#include <cstdint>

#include "animation/types.h"

namespace core::animation {

/// Semantic color slots that animations can reference instead of hardcoded colors.
/// Slots prefixed with User* are derived from the user's favorite hue at runtime.
/// Theme* slots are fixed brand colors.
enum class PaletteSlot : uint8_t {
  UserPrimary,     // user's favorite color, full saturation
  UserLight,       // same hue, higher value, lower saturation
  UserDark,        // same hue, lower value
  UserComplement,  // +180 degrees on the hue wheel
  UserAnalogous1,  // +30 degrees
  UserAnalogous2,  // -30 degrees
  ThemeGreen,      // solarpunk green (fixed)
  ThemeGold,       // solarpunk gold/amber (fixed)
  ThemeWhite,      // neutral warm white (fixed)
  Count,
};

inline constexpr uint8_t kPaletteSlotCount = static_cast<uint8_t>(PaletteSlot::Count);

/// Pre-resolved palette: concrete RGBF for each slot, computed once per animation start.
struct ResolvedPalette {
  RGBF colors[kPaletteSlotCount];

  const RGBF &operator[](PaletteSlot slot) const {
    return colors[static_cast<uint8_t>(slot)];
  }

  RGBF &operator[](PaletteSlot slot) {
    return colors[static_cast<uint8_t>(slot)];
  }
};

/// Convert RGB (0-255 per channel) to hue in degrees (0-360).
/// Returns 0 for achromatic colors.
inline uint16_t rgbToHueDeg(uint8_t r, uint8_t g, uint8_t b) {
  float redNorm = r / 255.0f;
  float greenNorm = g / 255.0f;
  float blueNorm = b / 255.0f;

  float maxChannel =
      redNorm > greenNorm ? (redNorm > blueNorm ? redNorm : blueNorm) : (greenNorm > blueNorm ? greenNorm : blueNorm);
  float minChannel =
      redNorm < greenNorm ? (redNorm < blueNorm ? redNorm : blueNorm) : (greenNorm < blueNorm ? greenNorm : blueNorm);
  float delta = maxChannel - minChannel;

  if (delta < 0.001f) {
    return 0;  // achromatic
  }

  float hue;

  if (maxChannel == redNorm) {
    hue = 60.0f * fmodf((greenNorm - blueNorm) / delta, 6.0f);
  } else if (maxChannel == greenNorm) {
    hue = 60.0f * ((blueNorm - redNorm) / delta + 2.0f);
  } else {
    hue = 60.0f * ((redNorm - greenNorm) / delta + 4.0f);
  }

  if (hue < 0.0f) {
    hue += 360.0f;
  }

  return static_cast<uint16_t>(hue + 0.5f);
}

/// Resolve all palette slots from an RGB user color (0-255 per channel).
/// UserPrimary faithfully reproduces the chosen color; derived slots use its
/// hue but keep their own fixed saturation/value so they remain visually
/// distinct even for low-saturation inputs like white.
inline ResolvedPalette resolvePalette(uint8_t r, uint8_t g, uint8_t b) {
  ResolvedPalette palette;

  // UserPrimary is the user's chosen color, verbatim.
  palette[PaletteSlot::UserPrimary] = {r / 255.0f, g / 255.0f, b / 255.0f};

  // Derive hue for the remaining slots.
  float hueNormalized = static_cast<float>(rgbToHueDeg(r, g, b)) / 360.0f;

  palette[PaletteSlot::UserLight] = hsvToRgb(hueNormalized, 0.4f, 1.0f);
  palette[PaletteSlot::UserDark] = hsvToRgb(hueNormalized, 0.9f, 0.4f);
  palette[PaletteSlot::UserComplement] = hsvToRgb(hueNormalized + 0.5f, 0.9f, 1.0f);
  palette[PaletteSlot::UserAnalogous1] = hsvToRgb(hueNormalized + 30.0f / 360.0f, 0.9f, 1.0f);
  palette[PaletteSlot::UserAnalogous2] = hsvToRgb(hueNormalized - 30.0f / 360.0f, 0.9f, 1.0f);

  // Fixed theme colors
  palette[PaletteSlot::ThemeGreen] = {0.18f, 0.85f, 0.30f};
  palette[PaletteSlot::ThemeGold] = {1.0f, 0.75f, 0.15f};
  palette[PaletteSlot::ThemeWhite] = {1.0f, 0.95f, 0.85f};

  return palette;
}

}  // namespace core::animation
