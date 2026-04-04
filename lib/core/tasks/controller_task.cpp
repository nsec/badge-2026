#include "tasks/controller_task.h"

#include <Arduino.h>
#include "hardware/serial_mutex.h"
#include <cstring>

#include "tasks/display.h"
#include "storage/nvs_social.h"
#include "storage/nvs_wifi_creds.h"
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
         storage::socialRead(storage::SocialKey::Light) == 255;
}

// ---------------------------------------------------------------------------
// Button handling
//
// A     = cycle through social categories
// B     = show social progress (double-press = hold LEDs on)
// DOWN  = NFC P2P pair
// LEFT  = NFC read
// RIGHT = NFC emulate
// UP    = (reserved — not yet assigned)
// ---------------------------------------------------------------------------

static constexpr storage::SocialKey SOCIAL_ORDER[] = {
    storage::SocialKey::Social,   // purple — citizens/players
    storage::SocialKey::Sponsor,  // green — vendors
    storage::SocialKey::Light,    // blue — light collection
};
static constexpr uint8_t SOCIAL_COUNT = sizeof(SOCIAL_ORDER) / sizeof(SOCIAL_ORDER[0]);

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

    // --- UP: reserved (not yet assigned) ---
    case hw::Button::Up: {
      // TODO: assign a function to the UP button
      break;
    }

    // --- A: cycle through social categories ---
    case hw::Button::A: {
      _nfcMode = NfcMode::Off;
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      _socialIndex = (_socialIndex + 1) % SOCIAL_COUNT;
      _lastButton = hw::Button::A;

      if (_socialActive) {
        showCurrentSocial(_holdActive);

        DisplayCommand dc{};
        dc.type = DisplayCommand::Type::SocialProgress;
        dc.socialKey = static_cast<uint8_t>(SOCIAL_ORDER[_socialIndex]);
        dc.socialValue = storage::socialRead(SOCIAL_ORDER[_socialIndex]);
        _displayQueue.send(dc, Milliseconds(0));
      }
      break;
    }

    // --- B: show current social category (double-press = hold) ---
    case hw::Button::B: {
      _nfcMode = NfcMode::Off;
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      _socialActive = true;

      bool hold = false;
      if (_lastButton == hw::Button::B) {
        _holdActive = !_holdActive;
        hold = _holdActive;
      } else {
        _holdActive = false;
      }
      _lastButton = hw::Button::B;

      showCurrentSocial(hold);

      DisplayCommand dc{};
      dc.type = DisplayCommand::Type::SocialProgress;
      dc.socialKey = static_cast<uint8_t>(SOCIAL_ORDER[_socialIndex]);
      dc.socialValue = storage::socialRead(SOCIAL_ORDER[_socialIndex]);
      _displayQueue.send(dc, Milliseconds(0));
      break;
    }

    // --- LEFT: NFC read (toggle) ---
    case hw::Button::Left: {
      _lastButton = hw::Button::Left;
      NfcMode target = (_nfcMode == NfcMode::Reader) ? NfcMode::Off : NfcMode::Reader;
      _nfcMode = target;
      _nfcQueue.send(NfcCommand{target}, Milliseconds(0));
      DisplayCommand dc{};
      if (target == NfcMode::Off) {
        dc.type = DisplayCommand::Type::ShowLogo;
      } else {
        dc.type = DisplayCommand::Type::ModeChange;
        dc.nfcMode = static_cast<uint8_t>(target);
      }
      _displayQueue.send(dc, Milliseconds(0));
      break;
    }

    // --- RIGHT: NFC emulate (toggle) ---
    case hw::Button::Right: {
      _lastButton = hw::Button::Right;
      NfcMode target = (_nfcMode == NfcMode::Emulator) ? NfcMode::Off : NfcMode::Emulator;
      _nfcMode = target;
      _nfcQueue.send(NfcCommand{target}, Milliseconds(0));
      DisplayCommand dc{};
      if (target == NfcMode::Off) {
        dc.type = DisplayCommand::Type::ShowLogo;
      } else {
        dc.type = DisplayCommand::Type::ModeChange;
        dc.nfcMode = static_cast<uint8_t>(target);
      }
      _displayQueue.send(dc, Milliseconds(0));
      break;
    }

    // --- DOWN: NFC P2P pair (toggle) ---
    case hw::Button::Down: {
      _lastButton = hw::Button::Down;
      NfcMode target = (_nfcMode == NfcMode::Pair) ? NfcMode::Off : NfcMode::Pair;
      _nfcMode = target;
      _nfcQueue.send(NfcCommand{target}, Milliseconds(0));
      DisplayCommand dc{};
      if (target == NfcMode::Off) {
        dc.type = DisplayCommand::Type::ShowLogo;
      } else {
        dc.type = DisplayCommand::Type::ModeChange;
        dc.nfcMode = static_cast<uint8_t>(target);
      }
      _displayQueue.send(dc, Milliseconds(0));
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

  core::hw::safeSerial().printf("[social] SET %s = %u (stored with HWID XOR)\r\n", storage::socialKeyName(req.key),
                                req.value);

  _cliQueue.send(CliResponse{CliResponseType::SocialSetComplete});
}

// ---------------------------------------------------------------------------
// WiFi portal toggle (A+B or CLI)
// ---------------------------------------------------------------------------

void ControllerTask::handle(const PortalToggleRequest &) {
  _portalActive = !_portalActive;

  if (_portalActive) {
    auto creds = storage::wifiCredsGet();

    PortalCommand pcmd{};
    pcmd.type = PortalCommand::Type::Start;
    strlcpy(pcmd.start.ssid, creds.ssid, sizeof(pcmd.start.ssid));
    strlcpy(pcmd.start.passphrase, creds.passphrase, sizeof(pcmd.start.passphrase));
    _portalQueue.send(pcmd);

    // Start NFC WiFi emulation so phones can tap the badge to get a
    // "Connect to Wi-Fi?" prompt.
    NfcCommand ncmd{};
    ncmd.mode = NfcMode::WifiEmulator;
    strlcpy(ncmd.wifi.ssid, creds.ssid, sizeof(ncmd.wifi.ssid));
    strlcpy(ncmd.wifi.passphrase, creds.passphrase, sizeof(ncmd.wifi.passphrase));
    _nfcQueue.send(ncmd, Milliseconds(0));
  } else {
    PortalCommand pcmd{};
    pcmd.type = PortalCommand::Type::Stop;
    _portalQueue.send(pcmd);

    _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
  }

  Serial.printf("[controller] Portal %s\r\n", _portalActive ? "starting" : "stopping");
}

// ---------------------------------------------------------------------------
// Config changed (from WiFi portal) — update LEDs to reflect new settings
// ---------------------------------------------------------------------------

void ControllerTask::handle(const ConfigChangedEvent &event) {
  const auto &cfg = event.config;

  hw::rgbSetBrightness(cfg.brightness);

  LedCommand cmd{};
  cmd.type = LedCommandType::SolidColor;
  cmd.r = cfg.profile.r;
  cmd.g = cfg.profile.g;
  cmd.b = cfg.profile.b;
  _ledQueue.send(cmd);

  Serial.printf("[controller] Config updated: name=%s bright=%u color=(%u,%u,%u)\r\n", cfg.profile.name, cfg.brightness,
                cfg.profile.r, cfg.profile.g, cfg.profile.b);
}

}  // namespace core
