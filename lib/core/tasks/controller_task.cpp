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
    _ledQueue.send(LedCommand{sequence[i]});
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  _cliQueue.send(CliResponse{CliResponseType::LedTestComplete});
}

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------

uint8_t ControllerTask::valueToPixelCount(uint8_t value) {
  if (value == 0)
    return 1;
  uint8_t count = static_cast<uint8_t>(1 + (static_cast<uint16_t>(value) * 17) / 255);
  if (count > hw::RGB_LED_COUNT)
    count = hw::RGB_LED_COUNT;
  return count;
}

bool ControllerTask::allSocialMaxed() {
  return storage::socialRead(storage::SocialKey::Social) == 255 &&
         storage::socialRead(storage::SocialKey::Sponsor) == 255 &&
         storage::socialRead(storage::SocialKey::Light) == 255 &&
         storage::socialRead(storage::SocialKey::Attraction) == 255;
}

// ---------------------------------------------------------------------------
// Button handling — new layout
//
// UP    = show social progress (double-press = hold LEDs on)
// LEFT  = cycle through 4 social categories
// RIGHT = cycle brightness (10 levels)
// DOWN  = NFC emulate
// A     = NFC read
// B     = NFC P2P pair (press again to cancel)
// ---------------------------------------------------------------------------

static constexpr storage::SocialKey SOCIAL_ORDER[] = {
    storage::SocialKey::Social,      // purple — citizens/players
    storage::SocialKey::Sponsor,     // green — vendors
    storage::SocialKey::Light,       // blue — light collection
    storage::SocialKey::Attraction,  // yellow — attractions
};
static constexpr const char *SOCIAL_NAMES[] = {"social", "sponsor", "light", "attraction"};

void ControllerTask::socialColor(storage::SocialKey key, uint8_t &r, uint8_t &g, uint8_t &b) {
  switch (key) {
    case storage::SocialKey::Social:
      r = 128;
      g = 0;
      b = 255;
      break;  // purple
    case storage::SocialKey::Sponsor:
      r = 0;
      g = 255;
      b = 0;
      break;  // green
    case storage::SocialKey::Light:
      r = 0;
      g = 0;
      b = 255;
      break;  // blue
    case storage::SocialKey::Attraction:
      r = 255;
      g = 255;
      b = 0;
      break;  // yellow
    default:
      r = 255;
      g = 255;
      b = 255;
      break;
  }
}

void ControllerTask::showCurrentSocial(bool hold) {
  storage::SocialKey key = SOCIAL_ORDER[_socialIndex];
  uint8_t r, g, b;
  socialColor(key, r, g, b);

  // If all maxed, rainbow instead
  if (allSocialMaxed()) {
    LedCommand cmd{};
    cmd.type = LedCommandType::Rainbow;
    cmd.hold = hold;
    _ledQueue.send(cmd);
    return;
  }

  uint8_t value = storage::socialRead(key);
  uint8_t pixels = valueToPixelCount(value);

  // Serial.printf("[social] showing %s = %u → %u LEDs%s\r\n", SOCIAL_NAMES[_socialIndex], value, pixels,
  //               hold ? " (hold)" : "");

  LedCommand cmd{};
  cmd.type = LedCommandType::ProgressFlash;
  cmd.pixelCount = pixels;
  cmd.r = r;
  cmd.g = g;
  cmd.b = b;
  cmd.hold = hold;
  _ledQueue.send(cmd);
}

void ControllerTask::handle(const ButtonPressEvent &event) {
  switch (event.button) {

    // --- UP: show current social category ---
    case hw::Button::Up: {
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      _socialActive = true;

      // Double-press toggle for hold
      bool hold = false;
      if (_lastButton == hw::Button::Up) {
        _holdActive = !_holdActive;
        hold = _holdActive;
      } else {
        _holdActive = false;
      }
      _lastButton = hw::Button::Up;

      showCurrentSocial(hold);
      break;
    }

    // --- LEFT: cycle through social categories ---
    case hw::Button::Left: {
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      _socialIndex = (_socialIndex + 1) % 4;
      _lastButton = hw::Button::Left;

      // Serial.printf("[social] switched to: %s\r\n", SOCIAL_NAMES[_socialIndex]);

      // If social display was active (UP was pressed), show immediately
      if (_socialActive) {
        showCurrentSocial(_holdActive);
      }
      break;
    }

    // --- RIGHT: cycle brightness (10 levels) ---
    case hw::Button::Right: {
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      _brightnessLevel++;
      if (_brightnessLevel > 10)
        _brightnessLevel = 1;
      _lastButton = hw::Button::Right;

      // Map 1-10 to brightness: level 1 = 25, level 10 = 255
      uint8_t brightness = static_cast<uint8_t>(25 + (_brightnessLevel - 1) * (230 / 9));

      hw::rgbSetBrightness(brightness);
      // Serial.printf("[brightness] level %u/10 (raw=%u)\r\n", _brightnessLevel, brightness);

      // Re-show the current social display at the new brightness
      if (_socialActive) {
        showCurrentSocial(_holdActive);
      }
      break;
    }

    // --- DOWN: NFC emulate ---
    case hw::Button::Down: {
      _lastButton = hw::Button::Down;
      _nfcQueue.send(NfcCommand{NfcMode::Emulator}, Milliseconds(0));
      break;
    }

    // --- A: NFC read ---
    case hw::Button::A: {
      _lastButton = hw::Button::A;
      _nfcQueue.send(NfcCommand{NfcMode::Reader}, Milliseconds(0));
      break;
    }

    // --- B: NFC P2P pair ---
    case hw::Button::B: {
      _lastButton = hw::Button::B;
      _nfcQueue.send(NfcCommand{NfcMode::Pair}, Milliseconds(0));
      break;
    }

    default:
      break;
  }
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
