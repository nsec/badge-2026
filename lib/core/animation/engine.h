#pragma once

#include "animation/behaviors.h"
#include "animation/field.h"
#include "animation/palette.h"
#include "animation/types.h"
#include "hardware/rgb_led.h"

namespace core::animation {

/// A track describes one virtual light's behavior over time.
struct LightTrack {
  Motion motion;
  Color color;
  Scalar radius;
  Scalar intensity;
  float startTime = 0;  // seconds before this track begins contributing
  float fadeIn = 0;     // seconds to ramp from 0 to full intensity after startTime
};

/// An animation definition: a name, duration, and up to kMaxLights tracks.
struct AnimationDef {
  char name[32];
  float duration;  // seconds. 0 = loop forever.
  LightTrack tracks[kMaxLights];
  uint8_t trackCount;
};

/// Drives an AnimationDef, evaluating behaviors each frame and writing to the LED strip.
class AnimationEngine {
public:
  /// Start playing an animation with a resolved palette for PaletteSlot colors.
  void start(const AnimationDef &def, const ResolvedPalette &palette);

  /// Stop the current animation and clear LEDs.
  void stop();

  bool isRunning() const {
    return _running;
  }

  /// Advance one frame. Call at 60 fps.
  /// Writes to rgbSetPixel/rgbShow. Returns false if the animation has ended.
  bool tick(float dt);

private:
  const AnimationDef *_def = nullptr;
  float _elapsed = 0;
  uint8_t _frameCounter = 0;
  bool _running = false;
  ResolvedPalette _palette;
  Light _lights[kMaxLights];
  RGBF _framebuffer[hw::RGB_LED_COUNT];
};

}  // namespace core::animation
