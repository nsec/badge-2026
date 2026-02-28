#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

// Forward-declare LedCommandType so the progress callback can reference it.
enum class LedCommandType : uint8_t;

using ProgressFn = void (*)(uint8_t step, uint8_t total, LedCommandType animation);

enum class ControllerEventType : uint8_t { LedTestRequest };

struct ControllerEvent {
  ControllerEventType type;
  union {
    struct {
      ProgressFn progress;
    } ledTest;
  };
};

extern Queue<ControllerEvent> *g_controllerQueue;

}  // namespace core
