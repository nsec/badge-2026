#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

// --- LED command types ---

enum class LedCommandType : uint8_t {
  PlayAnimation,
  SetSolidColor,
  SetBrightness,
  Clear,
};

struct LedCommand {
  LedCommandType type;
  union {
    struct {
      uint8_t animationId;
    } play;
    struct {
      uint8_t r, g, b;
    } color;
    struct {
      uint8_t brightness;
    } brightness;
  };
};

extern Queue<LedCommand> *g_ledQueue;

}  // namespace core
