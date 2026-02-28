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
};

struct LedCommand {
  LedCommandType type;
};

extern Queue<LedCommand> *g_ledQueue;

}  // namespace core
