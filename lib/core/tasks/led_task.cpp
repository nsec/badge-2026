#include "tasks/led_task.h"

#include <memory>

#include <Arduino.h>

#include "animation/palette.h"
#include "animation/types.h"
#include "hardware/rgb_led.h"
#include "storage/nvs_config.h"

namespace core {

namespace led {

void solidColor(uint8_t r, uint8_t g, uint8_t b) {
  hw::rgbSetAll(r, g, b);
  delay(1000);
}

void pixelWalk() {
  for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
    hw::rgbClear();
    hw::rgbSetPixel(i, 255, 0, 255);
    hw::rgbShow();
    delay(100);
  }
}

void rainbow() {
  for (int frame = 0; frame < 256; frame += 4) {
    for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
      uint8_t hue = (frame + i * 256 / hw::RGB_LED_COUNT) & 0xFF;
      uint8_t r, g, b;
      uint8_t region = hue / 43;
      uint8_t remainder = (hue - region * 43) * 6;
      switch (region) {
        case 0:
          r = 255;
          g = remainder;
          b = 0;
          break;
        case 1:
          r = 255 - remainder;
          g = 255;
          b = 0;
          break;
        case 2:
          r = 0;
          g = 255;
          b = remainder;
          break;
        case 3:
          r = 0;
          g = 255 - remainder;
          b = 255;
          break;
        case 4:
          r = remainder;
          g = 0;
          b = 255;
          break;
        default:
          r = 255;
          g = 0;
          b = 255 - remainder;
          break;
      }
      hw::rgbSetPixel(i, r, g, b);
    }
    hw::rgbShow();
    delay(20);
  }
}

void off() {
  hw::rgbClear();
  delay(500);
}

}  // namespace led

Queue<LedCommand> *g_ledQueue = nullptr;

bool LedTask::sleepOrInterrupt(uint32_t ms, LedCommand &out) {
  return _queue.receive(out, Milliseconds(ms));
}

bool LedTask::runProgressFlash(const LedCommand &cmd, LedCommand &out) {
  uint8_t ledCount = cmd.pixelCount;

  if (ledCount == 0) {
    ledCount = 1;
  }

  if (ledCount > hw::RGB_LED_COUNT) {
    ledCount = hw::RGB_LED_COUNT;
  }

  // 3 flashes: 250 ms on, 250 ms off
  for (int flash = 0; flash < 3; flash++) {
    hw::rgbClear();

    for (uint8_t i = 0; i < ledCount; i++) {
      hw::rgbSetPixel(i, cmd.r, cmd.g, cmd.b);
    }

    hw::rgbShow();

    if (sleepOrInterrupt(250, out)) {
      return true;
    }

    hw::rgbClear();

    if (sleepOrInterrupt(250, out)) {
      return true;
    }
  }

  // Solid for 3 seconds
  for (uint8_t i = 0; i < ledCount; i++) {
    hw::rgbSetPixel(i, cmd.r, cmd.g, cmd.b);
  }

  hw::rgbShow();

  if (sleepOrInterrupt(3000, out)) {
    return true;
  }

  // If hold, leave LEDs on (task will block on queue until next command).
  // Otherwise turn off.
  if (!cmd.hold) {
    hw::rgbClear();
  }

  return false;
}

bool LedTask::runRainbow(const LedCommand &cmd, LedCommand &out) {
  // hold=true: loop forever until interrupted.  hold=false: single loop then off.
  do {
    for (int frame = 0; frame < 256; frame += 4) {
      for (uint8_t i = 0; i < hw::RGB_LED_COUNT; i++) {
        uint8_t hue = (frame + i * 256 / hw::RGB_LED_COUNT) & 0xFF;
        uint8_t r, g, b;
        uint8_t region = hue / 43;
        uint8_t remainder = (hue - region * 43) * 6;
        switch (region) {
          case 0:
            r = 255;
            g = remainder;
            b = 0;
            break;
          case 1:
            r = 255 - remainder;
            g = 255;
            b = 0;
            break;
          case 2:
            r = 0;
            g = 255;
            b = remainder;
            break;
          case 3:
            r = 0;
            g = 255 - remainder;
            b = 255;
            break;
          case 4:
            r = remainder;
            g = 0;
            b = 255;
            break;
          default:
            r = 255;
            g = 0;
            b = 255 - remainder;
            break;
        }
        hw::rgbSetPixel(i, r, g, b);
      }
      hw::rgbShow();
      if (sleepOrInterrupt(20, out)) {
        hw::rgbClear();
        return true;
      }
    }
  } while (cmd.hold);

  hw::rgbClear();
  return false;
}

bool LedTask::runAnimation(LedCommand &cmd, LedCommand &out) {
  // Take ownership of the heap-allocated AnimationDef from the command.
  _currentAnimation.reset(cmd.animation);
  cmd.animation = nullptr;

  if (!_currentAnimation || _currentAnimation->trackCount == 0) {
    return false;
  }

  auto cfg = storage::configRead();
  auto palette = animation::resolvePalette(cfg.profile.r, cfg.profile.g, cfg.profile.b);

  _animEngine.start(*_currentAnimation, palette);

  while (_animEngine.isRunning()) {
    _animEngine.tick(animation::kFrameInterval);

    if (sleepOrInterrupt(animation::kFrameIntervalMs, out)) {
      if (out.type == LedCommandType::PaletteUpdate) {
        _animEngine.setPalette(animation::resolvePalette(out.r, out.g, out.b));
        continue;
      }

      _animEngine.stop();
      return true;
    }
  }

  return false;
}

void LedTask::run() {
  LedCommand cmd;
  bool pending = false;  // true if `cmd` already holds the next command

  for (;;) {
    if (!pending) {
      if (!_queue.receive(cmd)) {
        continue;
      }
    }

    pending = false;

    switch (cmd.type) {
      case LedCommandType::SolidRed:
        led::solidColor(255, 0, 0);
        break;
      case LedCommandType::SolidGreen:
        led::solidColor(0, 255, 0);
        break;
      case LedCommandType::SolidBlue:
        led::solidColor(0, 0, 255);
        break;
      case LedCommandType::SolidWhite:
        led::solidColor(255, 255, 255);
        break;
      case LedCommandType::SolidOrange:
        led::solidColor(255, 80, 0);
        break;
      case LedCommandType::SolidCyan:
        led::solidColor(0, 255, 255);
        break;
      case LedCommandType::PixelWalk:
        led::pixelWalk();
        break;
      case LedCommandType::Rainbow:
        pending = runRainbow(cmd, cmd);
        break;
      case LedCommandType::Off:
        led::off();
        break;
      case LedCommandType::ProgressFlash:
        pending = runProgressFlash(cmd, cmd);
        break;
      case LedCommandType::SolidColor:
        hw::rgbSetAll(cmd.r, cmd.g, cmd.b);
        break;
      case LedCommandType::Animation:
        pending = runAnimation(cmd, cmd);
        break;
      case LedCommandType::PaletteUpdate:
        // Only meaningful during an animation; ignore when idle.
        break;
    }
  }
}

}  // namespace core
