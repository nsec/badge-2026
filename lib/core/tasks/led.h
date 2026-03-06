#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

enum class LedCommandType : uint8_t {
  SolidRed,
  SolidGreen,
  SolidBlue,
  SolidWhite,
  PixelWalk,
  Rainbow,
  Off,
  ProgressFlash,  // show social-progress animation
};

struct LedCommand {
  LedCommand() = default;

  explicit LedCommand(LedCommandType type_) : type(type_) {}

  LedCommandType type = LedCommandType::Off;
  // Fields used only by ProgressFlash:
  uint8_t pixelCount = 1;       // how many of the 18 LEDs to light (1-18)
  uint8_t r = 0, g = 0, b = 0;  // colour
  bool hold = false;            // if true, keep LEDs on after animation (double-press)
};

extern Queue<LedCommand> *g_ledQueue;

}  // namespace core
