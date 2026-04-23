#include "animation/engine.h"

#include <badge_config.h>

#include "animation/field.h"
#include "animation/preview.h"
#include "hardware/rgb_led.h"

namespace core::animation {

PreviewFrame g_previewFrame;

void AnimationEngine::start(const AnimationDef &def, const ResolvedPalette &palette) {
  _def = &def;
  _palette = palette;
  _elapsed = 0;
  _frameCounter = 0;
  _running = true;
}

void AnimationEngine::stop() {
  _running = false;
  _def = nullptr;
  hw::rgbClear();
}

bool AnimationEngine::tick(float dt) {
  if (!_running || !_def) {
    return false;
  }

  _elapsed += dt;

  // Check if a finite animation has ended.
  if (_def->duration > 0 && _elapsed >= _def->duration) {
    stop();
    return false;
  }

  // For looping animations with a duration of 0, time just keeps advancing.
  float elapsed = _elapsed;

  // Evaluate each track's behaviors into runtime Light structs.
  for (uint8_t i = 0; i < _def->trackCount; i++) {
    const LightTrack &track = _def->tracks[i];

    // Track hasn't started yet — zero contribution.
    if (elapsed < track.startTime) {
      _lights[i].intensity = 0;
      continue;
    }

    float trackTime = elapsed - track.startTime;
    _lights[i].pos = evaluate(track.motion, trackTime);
    _lights[i].color = evaluate(track.color, trackTime, _palette);
    _lights[i].radius = evaluate(track.radius, trackTime);
    _lights[i].intensity = powf(evaluate(track.intensity, trackTime),
                                badge::config::animation::intensity_curve / badge::config::animation::gamma);

    // Apply fade-in envelope. The linear ramp is raised to 1/gamma so
    // that after gamma correction in the output stage the perceived
    // brightness increases linearly with time.
    if (track.fadeIn > 0 && trackTime < track.fadeIn) {
      float fade = trackTime / track.fadeIn;
      _lights[i].intensity *= powf(fade, 1.0f / badge::config::animation::gamma);
    }
  }

  // Publish light state for the preview visualizer.
  g_previewFrame.lightCount = _def->trackCount;

  for (uint8_t i = 0; i < _def->trackCount; i++) {
    g_previewFrame.lights[i] = _lights[i];
  }

  // Evaluate the light field at each LED position.
  evaluateField(_lights, _def->trackCount, _framebuffer);

  // Write to hardware with temporal dithering.
  for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
    const RGBF &pixel = _framebuffer[i];
    uint8_t red = ditheredOutput(pixel.r, badge::config::animation::white_balance_r, _frameCounter, i, 0);
    uint8_t green = ditheredOutput(pixel.g, badge::config::animation::white_balance_g, _frameCounter, i, 1);
    uint8_t blue = ditheredOutput(pixel.b, badge::config::animation::white_balance_b, _frameCounter, i, 2);
    hw::rgbSetPixel(i, red, green, blue);
  }

  hw::rgbShow();
  _frameCounter++;

  return true;
}

}  // namespace core::animation
