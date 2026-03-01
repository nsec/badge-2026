#include "tasks/controller_task.h"

#include <Arduino.h>

#include "storage/nvs_social.h"
#include "hardware/rgb_led.h"

namespace core {

Queue<ControllerEvent> *g_controllerQueue = nullptr;

void ControllerTask::run() {
  for (;;) {
    ControllerEvent event;
    if (!_inQueue.receive(event))
      continue;

    std::visit(
        [this](const auto &data) {
          handle(data);
        },
        event);
  }
}

// ---------------------------------------------------------------------------
// LED test (existing)
// ---------------------------------------------------------------------------

void ControllerTask::handle(const LedTestRequest &req) {
  constexpr LedCommandType sequence[] = {
      LedCommandType::SolidRed,  LedCommandType::SolidGreen, LedCommandType::SolidBlue, LedCommandType::SolidWhite,
      LedCommandType::PixelWalk, LedCommandType::Rainbow,    LedCommandType::Off,
  };
  constexpr uint8_t count = sizeof(sequence) / sizeof(sequence[0]);
  const uint8_t total = req.testNum ? 1 : count;

  uint8_t step = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (req.testNum && *req.testNum != i + 1)
      continue;
    step++;
    if (req.progress) {
      req.progress(step, total, sequence[i]);
    }
    _ledQueue.send(LedCommand{sequence[i], 0, 0, 0, 0, false});
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  _cliQueue.send(CliResponse{CliResponseType::LedTestComplete});
}

// ---------------------------------------------------------------------------
// Button presses → read NVS, send LED progress animation
// ---------------------------------------------------------------------------

bool ControllerTask::buttonToSocial(hw::Button btn, storage::SocialKey &key, uint8_t &r, uint8_t &g, uint8_t &b) {
  switch (btn) {
    case hw::Button::Up:  // vendors → green
      key = storage::SocialKey::Sponsor;
      r = 0;
      g = 255;
      b = 0;
      return true;
    case hw::Button::Left:  // light collection → blue
      key = storage::SocialKey::Light;
      r = 0;
      g = 0;
      b = 255;
      return true;
    case hw::Button::Down:  // citizens/players → purple
      key = storage::SocialKey::Social;
      r = 128;
      g = 0;
      b = 255;
      return true;
    case hw::Button::Right:  // attractions → yellow
      key = storage::SocialKey::Attraction;
      r = 255;
      g = 255;
      b = 0;
      return true;
    default:
      return false;
  }
}

uint8_t ControllerTask::valueToPixelCount(uint8_t value) {
  // Map 0-254 → 1-18 LEDs.  0 still lights 1 LED so the user sees feedback.
  // 254 → 18, linear.
  if (value == 0)
    return 1;
  uint8_t count = static_cast<uint8_t>(1 + (static_cast<uint16_t>(value) * 17) / 254);
  if (count > hw::RGB_LED_COUNT)
    count = hw::RGB_LED_COUNT;
  return count;
}

bool ControllerTask::allSocialMaxed() {
  return storage::socialRead(storage::SocialKey::Social) == 254 &&
         storage::socialRead(storage::SocialKey::Sponsor) == 254 &&
         storage::socialRead(storage::SocialKey::Light) == 254 &&
         storage::socialRead(storage::SocialKey::Attraction) == 254;
}

void ControllerTask::handle(const ButtonPressEvent &event) {
  storage::SocialKey key;
  uint8_t r, g, b;

  if (!buttonToSocial(event.button, key, r, g, b))
    return;  // unmapped button (A/B) — ignore

  // Determine hold (toggle): same button cycles off→on→off, different button resets.
  bool hold = false;
  if (event.button == _lastButton) {
    // Same button again — toggle hold
    _holdActive = !_holdActive;
    hold = _holdActive;
  } else {
    // Different button — reset
    _holdActive = false;
    hold = false;
  }
  _lastButton = event.button;

  // If all four categories are maxed, show rainbow instead.
  if (allSocialMaxed()) {
    LedCommand cmd{};
    cmd.type = LedCommandType::Rainbow;
    cmd.hold = hold;
    _ledQueue.send(cmd);
    return;
  }

  uint8_t value = storage::socialRead(key);
  uint8_t pixels = valueToPixelCount(value);

  LedCommand cmd{};
  cmd.type = LedCommandType::ProgressFlash;
  cmd.pixelCount = pixels;
  cmd.r = r;
  cmd.g = g;
  cmd.b = b;
  cmd.hold = hold;
  _ledQueue.send(cmd);
}

// ---------------------------------------------------------------------------
// CLI set-value request → write NVS, then flash confirmation
// ---------------------------------------------------------------------------

void ControllerTask::handle(const SocialSetRequest &req) {
  storage::socialWrite(req.key, req.value);

  Serial.printf("[social] SET %s = %u (stored with HWID XOR)\r\n", storage::socialKeyName(req.key), req.value);

  _cliQueue.send(CliResponse{CliResponseType::SocialSetComplete});
}

}  // namespace core
