#pragma once

// LED positions normalized to [0,1] from badge SVG coordinates.
// Raw range: X [62.78 .. 156.13], Y [48.99 .. 151.41]
// Normalized: x = (raw_x - 62.78) / 93.35,  y = (raw_y - 48.99) / 102.42

#include "animation/types.h"
#include "hardware/rgb_led.h"

namespace core::animation {

inline constexpr Vec2 kLedPositions[hw::RGB_LED_COUNT] = {
    {0.011f, 0.492f},  // 0  — left lower
    {0.000f, 0.407f},  // 1  — left upper
    {0.064f, 0.212f},  // 2  — top-left arc
    {0.172f, 0.092f},  // 3  — top-left arc
    {0.296f, 0.000f},  // 4  — top-left arc
    {0.367f, 0.027f},  // 5  — top center
    {0.497f, 0.035f},  // 6  — top center
    {0.626f, 0.030f},  // 7  — top center
    {0.716f, 0.016f},  // 8  — top center
    {0.824f, 0.090f},  // 9  — top center
    {1.000f, 0.418f},  // 10 — right upper
    {0.990f, 0.495f},  // 11 — right lower
    {0.860f, 1.000f},  // 12 — bottom (right end)
    {0.697f, 0.999f},  // 13 — bottom
    {0.536f, 0.998f},  // 14 — bottom
    {0.393f, 0.999f},  // 15 — bottom
    {0.276f, 0.997f},  // 16 — bottom
    {0.137f, 0.997f},  // 17 — bottom (left end)
};

}  // namespace core::animation
