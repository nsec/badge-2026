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
  LedCommandType type;
  // Fields used only by ProgressFlash:
  uint8_t pixelCount;  // how many of the 18 LEDs to light (1-18)
  uint8_t r, g, b;     // colour
  bool hold;           // if true, keep LEDs on after animation (double-press)
};

extern Queue<LedCommand> *g_ledQueue;

}  // namespace core
