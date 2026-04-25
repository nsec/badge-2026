#include "animation/builders.h"

#include <cstring>

#include "animation/behaviors.h"
#include "animation/led_positions.h"
#include "hardware/rgb_led.h"

namespace core::animation {

std::unique_ptr<AnimationDef> buildProgressFlash(uint8_t pixelCount, RGBF color, bool loop) {
  if (pixelCount == 0) {
    pixelCount = 1;
  }

  if (pixelCount > kMaxLights) {
    pixelCount = kMaxLights;
  }

  if (pixelCount > hw::RGB_LED_COUNT) {
    pixelCount = hw::RGB_LED_COUNT;
  }

  auto def = std::make_unique<AnimationDef>();

  std::strncpy(def->name, "progress_flash", sizeof(def->name) - 1);
  def->name[sizeof(def->name) - 1] = '\0';

  // Held flashes loop forever; one-shot flashes run for a few breath cycles.
  def->duration = loop ? 0.0f : 4.0f;
  def->trackCount = pixelCount;

  for (uint8_t i = 0; i < pixelCount; i++) {
    LightTrack &track = def->tracks[i];

    track.motion = StaticMotion{kLedPositions[i]};
    track.color = StaticColor{color};
    // Radius < min inter-LED spacing keeps each pixel crisp (no bleed).
    track.radius = StaticScalar{0.07f};
    // Breathe between intensity 0.3 and 0.7 (base 0.5 ± 0.2). Period is short
    // enough to feel like an active acknowledgment, not an ambient effect.
    track.intensity = BreatheScalar{0.5f, 0.2f, 0.9f, 0.0f};
    track.startTime = 0.0f;
    track.fadeIn = 0.0f;
  }

  return def;
}

std::unique_ptr<AnimationDef> buildSolidBreathe(RGBF color) {
  auto def = std::make_unique<AnimationDef>();

  std::strncpy(def->name, "solid_breathe", sizeof(def->name) - 1);
  def->name[sizeof(def->name) - 1] = '\0';

  def->duration = 0.0f;  // loop until interrupted

  uint8_t trackCount = hw::RGB_LED_COUNT;
  if (trackCount > kMaxLights) {
    trackCount = kMaxLights;
  }
  def->trackCount = trackCount;

  // Decorrelate per-LED breathe phases using a golden-ratio step so each LED
  // sits at a different point in its breathe cycle, producing a soft sparkle
  // rather than a synchronised pulse.
  constexpr float kPhaseStep = 2.39996323f;  // 2π * (1 - 1/φ)

  for (uint8_t i = 0; i < trackCount; i++) {
    LightTrack &track = def->tracks[i];

    track.motion = StaticMotion{kLedPositions[i]};
    track.color = StaticColor{color};
    track.radius = StaticScalar{0.07f};
    // Subtle envelope (0.6..1.0): the colour still reads as "solid" but is
    // never quite still.
    track.intensity = BreatheScalar{0.7f, 0.3f, 2.5f, i * kPhaseStep};
    track.startTime = 0.0f;
    track.fadeIn = 0.0f;
  }

  return def;
}

}  // namespace core::animation
