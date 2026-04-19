#pragma once

#include <cstdint>

#include "animation/engine.h"
#include "rtos/queue.hpp"

namespace core {

enum class LedCommandType : uint8_t {
  SolidRed,
  SolidGreen,
  SolidBlue,
  SolidWhite,
  SolidOrange,
  SolidCyan,
  PixelWalk,
  Rainbow,
  Off,
  ProgressFlash,  // show social-progress animation
  SolidColor,     // set all LEDs to the r/g/b fields and hold
  Animation,      // play a field-based animation from a heap-allocated AnimationDef
};

struct LedCommand {
  LedCommand() = default;

  explicit LedCommand(LedCommandType type_) : type(type_) {}

  LedCommandType type = LedCommandType::Off;
  // Fields used only by ProgressFlash:
  uint8_t pixelCount = 1;       // how many of the 18 LEDs to light (1-18)
  uint8_t r = 0, g = 0, b = 0;  // colour
  bool hold = false;            // if true, keep LEDs on after animation (double-press)
  // For Animation type: heap-allocated AnimationDef.
  // Ownership is transferred to the LedTask when the command is received.
  // The sender must allocate with new (e.g. via unique_ptr::release()).
  animation::AnimationDef *animation = nullptr;
};

extern Queue<LedCommand> *g_ledQueue;

}  // namespace core
