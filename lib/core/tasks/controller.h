#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "rtos/queue.hpp"

namespace core {

// Forward-declare LedCommandType so the progress callback can reference it.
enum class LedCommandType : uint8_t;

using ProgressFn = void (*)(uint8_t step, uint8_t total, LedCommandType animation);

struct LedTestRequest {
  ProgressFn progress = nullptr;
  std::optional<uint8_t> testNum;  // nullopt = run all, 1-7 = single
};

using ControllerEvent = std::variant<LedTestRequest>;

extern Queue<ControllerEvent> *g_controllerQueue;

}  // namespace core
