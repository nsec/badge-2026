#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

enum class CliResponseType : uint8_t { LedTestComplete };

struct CliResponse {
  CliResponseType type;
};

extern Queue<CliResponse> *g_cliQueue;

}  // namespace core
