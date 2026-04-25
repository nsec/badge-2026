#pragma once

#include <memory>

#include "animation/engine.h"
#include "animation/types.h"

namespace core::animation {

/// Build a smooth progress-flash animation: one static light per LED for the
/// first `pixelCount` LEDs, each tightly localized (small radius) so colors do
/// not bleed into neighbours, with a breathing intensity envelope.
///
/// When `loop` is true the animation runs forever (use for held social-progress
/// display). Otherwise it runs for a few seconds and stops, clearing the LEDs.
std::unique_ptr<AnimationDef> buildProgressFlash(uint8_t pixelCount, RGBF color, bool loop);

}  // namespace core::animation
