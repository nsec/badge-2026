#include "tasks/controller_task.h"

#include <Arduino.h>
#include "hardware/serial_mutex.h"
#include <cstring>

#include "tasks/display.h"
#include "storage/nvs_social.h"
#include "storage/nvs_wifi_creds.h"
#include "hardware/rgb_led.h"
#include "animation/parser.h"
#include "animation/storage.h"

namespace core {

Queue<ControllerEvent> *g_controllerQueue = nullptr;

namespace {

// Reserved animation names — not eligible for the idle cycle. Boot is shown
// only at startup; wifi_portal is driven by the portal handler.
constexpr const char *kReservedAnimations[] = {"boot", "wifi_portal"};

bool isReservedAnimation(const std::string &name) {
  for (const char *reserved : kReservedAnimations) {
    if (name == reserved) {
      return true;
    }
  }
  return false;
}

// Per-state timeouts (0 = no timeout). On expiry the controller returns to
// Idle. Reader/Emulator are intentionally untimed — they are toggled off
// explicitly by the user.
constexpr uint32_t kSocialTimeoutMs = 5000;
constexpr uint32_t kPairTimeoutMs = 20000;
constexpr uint32_t kPortalTimeoutMs = 5UL * 60UL * 1000UL;  // 5 minutes

// How long to wait after a result flash (scan/pair) before queuing the
// idle animation. A touch longer than ProgressFlash's own duration so the
// flash plays out cleanly.
constexpr uint32_t kResultFlashMs = 4500;

constexpr storage::SocialKey SOCIAL_ORDER[] = {
    storage::SocialKey::Social,   // purple — citizens/players
    storage::SocialKey::Sponsor,  // green — vendors
    storage::SocialKey::Light,    // blue — light collection
};
constexpr uint8_t SOCIAL_COUNT = sizeof(SOCIAL_ORDER) / sizeof(SOCIAL_ORDER[0]);

}  // namespace

// ---------------------------------------------------------------------------
// Run loop
// ---------------------------------------------------------------------------

void ControllerTask::run() {
  enumerateIdleAnimations();

  // Initial state: play the boot animation as the first idle.
  enterIdle();

  for (;;) {
    ControllerEvent event;
    bool gotEvent;

    if (_timeoutAt == 0) {
      gotEvent = _inQueue.receive(event);
    } else {
      uint32_t now = millis();
      uint32_t waitMs = (_timeoutAt > now) ? (_timeoutAt - now) : 0;
      gotEvent = _inQueue.receive(event, Milliseconds(waitMs));
    }

    if (!gotEvent) {
      // Timer fired.
      if (_state == State::Idle) {
        // Deferred post-flash idle animation.
        _timeoutAt = 0;
        sendCurrentIdleAnimation();
      } else {
        // State-specific timeout — return to Idle.
        enterIdle();
      }
      continue;
    }

    std::visit(
        [this](const auto &data) {
          handle(data);
        },
        event);
  }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void ControllerTask::cleanupCurrent() {
  switch (_state) {
    case State::Idle:
      // Stop the idle URL emulator (started by enterIdle).
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      break;
    case State::Social:
      // No external resources to release.
      break;
    case State::NfcReader:
    case State::NfcEmulator:
    case State::NfcPair:
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));
      break;
    case State::Portal: {
      _nfcQueue.send(NfcCommand{NfcMode::Off}, Milliseconds(0));

      PortalCommand pcmd{};
      pcmd.type = PortalCommand::Type::Stop;
      _portalQueue.send(pcmd);
      break;
    }
  }
}

void ControllerTask::sendNamedAnimation(const char *name) {
  auto result = animation::loadAndParseAnimation(name);
  if (!result.ok) {
    core::hw::safeSerial().printf("[controller] failed to load animation '%s'\r\n", name);
    return;
  }

  LedCommand cmd(LedCommandType::Animation);
  cmd.animation = result.def.release();
  _ledQueue.send(cmd, Milliseconds(0));
}

void ControllerTask::sendCurrentIdleAnimation() {
  if (!_idleIndex || _idleAnimations.empty()) {
    sendNamedAnimation("boot");
  } else {
    sendNamedAnimation(_idleAnimations[*_idleIndex].c_str());
  }
}

void ControllerTask::enumerateIdleAnimations() {
  _idleAnimations.clear();

  for (const auto &name : animation::storageList()) {
    if (isReservedAnimation(name)) {
      continue;
    }

    _idleAnimations.push_back(name);
  }
}

void ControllerTask::enterIdle(uint32_t deferMs) {
  cleanupCurrent();
  _state = State::Idle;
  _holdActive = false;

  // Reset social-display tracking. _socialIndex is preserved so the user
  // returns to the last category they cycled to.
  _lastButton = hw::Button::COUNT;

  // ShowLogo on the e-ink to match the LED transition.
  DisplayCommand dc{};
  dc.type = DisplayCommand::Type::ShowLogo;
  _displayQueue.send(dc, Milliseconds(0));

  if (deferMs > 0) {
    _timeoutAt = millis() + deferMs;
    // The idle animation will be queued when the timer fires; until then we
    // let whatever is already on the LEDs (typically a result flash) play.
  } else {
    _timeoutAt = 0;
    sendCurrentIdleAnimation();
  }

  // On first boot, start the URL emulator so the badge is immediately
  // tappable. After that, NFC stays off in idle — the user can start
  // emulation explicitly with the Right button.
  if (!_hasLeftBoot) {
    _hasLeftBoot = true;
    _nfcQueue.send(NfcCommand{NfcMode::UrlEmulator}, Milliseconds(0));
    // Reflect the active emulator in state so pressing Right toggles it off
    // rather than stop+restart.
    _state = State::NfcEmulator;
  }
}

void ControllerTask::enterSocial() {
  cleanupCurrent();
  _state = State::Social;
  _timeoutAt = _holdActive ? 0 : millis() + kSocialTimeoutMs;

  showCurrentSocial();

  DisplayCommand dc{};
  dc.type = DisplayCommand::Type::SocialProgress;
  dc.socialKey = static_cast<uint8_t>(SOCIAL_ORDER[_socialIndex]);
  dc.socialValue = storage::socialRead(SOCIAL_ORDER[_socialIndex]);
  _displayQueue.send(dc, Milliseconds(0));
}

void ControllerTask::enterNfcMode(NfcMode mode) {
  // Leaving boot for the first time? Pin the idle index so the next idle
  // entry plays the cycle, not boot.
  if (!_idleIndex) {
    _idleIndex = 0;
  }

  cleanupCurrent();

  switch (mode) {
    case NfcMode::Reader:
      _state = State::NfcReader;
      _ledQueue.send(LedCommand{LedCommandType::SolidWhite}, Milliseconds(0));
      _timeoutAt = 0;
      break;
    case NfcMode::Emulator:
      _state = State::NfcEmulator;
      _ledQueue.send(LedCommand{LedCommandType::SolidCyan}, Milliseconds(0));
      _timeoutAt = 0;
      break;
    case NfcMode::Pair:
      _state = State::NfcPair;
      _ledQueue.send(LedCommand{LedCommandType::SolidOrange}, Milliseconds(0));
      _timeoutAt = millis() + kPairTimeoutMs;
      break;
    default:
      // Should not happen — WifiEmulator is driven by enterPortal.
      _ledQueue.send(LedCommand{LedCommandType::Off}, Milliseconds(0));
      _state = State::Idle;
      _timeoutAt = 0;
      return;
  }

  _nfcQueue.send(NfcCommand{mode}, Milliseconds(0));

  DisplayCommand dc{};
  dc.type = DisplayCommand::Type::ModeChange;
  dc.nfcMode = static_cast<uint8_t>(mode);
  _displayQueue.send(dc, Milliseconds(0));
}

void ControllerTask::enterPortal() {
  if (!_idleIndex) {
    _idleIndex = 0;
  }

  cleanupCurrent();
  _state = State::Portal;
  _timeoutAt = millis() + kPortalTimeoutMs;

  auto creds = storage::wifiCredsGet();

  PortalCommand pcmd{};
  pcmd.type = PortalCommand::Type::Start;
  strlcpy(pcmd.start.ssid, creds.ssid, sizeof(pcmd.start.ssid));
  strlcpy(pcmd.start.passphrase, creds.passphrase, sizeof(pcmd.start.passphrase));
  _portalQueue.send(pcmd);

  // Phones can tap the badge for a "Connect to Wi-Fi?" prompt while the
  // portal is up.
  NfcCommand ncmd{};
  ncmd.mode = NfcMode::WifiEmulator;
  strlcpy(ncmd.wifi.ssid, creds.ssid, sizeof(ncmd.wifi.ssid));
  strlcpy(ncmd.wifi.passphrase, creds.passphrase, sizeof(ncmd.wifi.passphrase));
  _nfcQueue.send(ncmd, Milliseconds(0));

  sendNamedAnimation("wifi_portal");

  Serial.println("[controller] Portal starting");
}

void ControllerTask::cycleIdleAnimation() {
  if (_idleAnimations.empty()) {
    return;
  }

  // First press leaves boot at index 0. Otherwise step to the next animation.
  _idleIndex = _idleIndex ? (*_idleIndex + 1) % _idleAnimations.size() : 0;
  sendNamedAnimation(_idleAnimations[*_idleIndex].c_str());
}

// ---------------------------------------------------------------------------
// LED test
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

void ControllerTask::showCurrentSocial() {
  storage::SocialKey key = SOCIAL_ORDER[_socialIndex];
  uint8_t r, g, b;
  socialColor(key, r, g, b);

  // All-maxed → rainbow celebration.
  if (allSocialMaxed()) {
    LedCommand cmd{};
    cmd.type = LedCommandType::Rainbow;
    cmd.hold = true;
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
  // Always hold — the timeout (or another transition) drives the exit.
  cmd.hold = true;
  _ledQueue.send(cmd);
}

// ---------------------------------------------------------------------------
// Button handling
//
// A     = cycle social category (in Social) / cycle idle animation (in Idle)
// B     = enter Social / toggle hold (B-double-press)
// UP    = toggle WiFi config portal
// DOWN  = toggle NFC P2P pair
// LEFT  = toggle NFC reader
// RIGHT = toggle NFC emulator
// ---------------------------------------------------------------------------

void ControllerTask::handle(const ButtonPressEvent &event) {
  switch (event.button) {

    case hw::Button::Up: {
      _lastButton = hw::Button::Up;
      if (_state == State::Portal) {
        enterIdle();
      } else {
        enterPortal();
      }
      break;
    }

    case hw::Button::A: {
      const hw::Button prev = _lastButton;
      _lastButton = hw::Button::A;

      if (_state == State::Idle) {
        cycleIdleAnimation();
        break;
      }

      if (_state == State::Social) {
        _socialIndex = (_socialIndex + 1) % SOCIAL_COUNT;
        // Reset the social timeout on user activity (no-op when held).
        if (!_holdActive) {
          _timeoutAt = millis() + kSocialTimeoutMs;
        }
        showCurrentSocial();

        DisplayCommand dc{};
        dc.type = DisplayCommand::Type::SocialProgress;
        dc.socialKey = static_cast<uint8_t>(SOCIAL_ORDER[_socialIndex]);
        dc.socialValue = storage::socialRead(SOCIAL_ORDER[_socialIndex]);
        _displayQueue.send(dc, Milliseconds(0));
        break;
      }

      // From any other state (NFC modes, Portal): A returns to Idle.
      (void)prev;
      enterIdle();
      break;
    }

    case hw::Button::B: {
      const bool wasB = (_lastButton == hw::Button::B);
      _lastButton = hw::Button::B;

      if (_state == State::Social && wasB) {
        // B-double-press toggles hold.
        _holdActive = !_holdActive;
        _timeoutAt = _holdActive ? 0 : millis() + kSocialTimeoutMs;
        showCurrentSocial();
        break;
      }

      enterSocial();
      break;
    }

    case hw::Button::Left: {
      _lastButton = hw::Button::Left;
      if (_state == State::NfcReader) {
        enterIdle();
      } else {
        enterNfcMode(NfcMode::Reader);
      }
      break;
    }

    case hw::Button::Right: {
      _lastButton = hw::Button::Right;
      if (_state == State::NfcEmulator) {
        enterIdle();
      } else {
        enterNfcMode(NfcMode::Emulator);
      }
      break;
    }

    case hw::Button::Down: {
      _lastButton = hw::Button::Down;
      if (_state == State::NfcPair) {
        enterIdle();
      } else {
        enterNfcMode(NfcMode::Pair);
      }
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
// WiFi portal toggle (CLI / external)
// ---------------------------------------------------------------------------

void ControllerTask::handle(const PortalToggleRequest &) {
  if (_state == State::Portal) {
    enterIdle();
  } else {
    enterPortal();
  }
}

// ---------------------------------------------------------------------------
// Config changed (from WiFi portal) — apply brightness/palette
// ---------------------------------------------------------------------------

void ControllerTask::handle(const ConfigChangedEvent &event) {
  const auto &cfg = event.config;

  hw::rgbSetBrightness(cfg.brightness);

  LedCommand cmd{};
  cmd.type = LedCommandType::PaletteUpdate;
  cmd.r = cfg.profile.r;
  cmd.g = cfg.profile.g;
  cmd.b = cfg.profile.b;
  _ledQueue.send(cmd);

  Serial.printf("[controller] Config updated: name=%s bright=%u color=(%u,%u,%u)\r\n", cfg.profile.name, cfg.brightness,
                cfg.profile.r, cfg.profile.g, cfg.profile.b);
}

// ---------------------------------------------------------------------------
// NFC reader scan complete — flash green, then drift back to Idle
// ---------------------------------------------------------------------------

void ControllerTask::handle(const NfcScanResultEvent &) {
  LedCommand cmd{};
  cmd.type = LedCommandType::ProgressFlash;
  cmd.pixelCount = 18;
  cmd.g = 255;
  _ledQueue.send(cmd, Milliseconds(0));

  // The NFC task auto-exits Reader after a successful scan. Defer the
  // idle animation so the green flash plays out first.
  enterIdle(kResultFlashMs);
}

// ---------------------------------------------------------------------------
// NFC-DEP pair complete — flash by outcome, then drift back to Idle
// ---------------------------------------------------------------------------

void ControllerTask::handle(const NfcPairResultEvent &event) {
  LedCommand cmd{};
  cmd.type = LedCommandType::ProgressFlash;
  cmd.pixelCount = 18;
  switch (event.outcome) {
    case 0:  // new partner — green
      cmd.g = 255;
      break;
    case 1:  // duplicate — yellow
      cmd.r = 255;
      cmd.g = 255;
      break;
    case 2:  // HMAC failed — red
      cmd.r = 255;
      break;
    default:
      break;
  }
  _ledQueue.send(cmd, Milliseconds(0));

  enterIdle(kResultFlashMs);
}

}  // namespace core
