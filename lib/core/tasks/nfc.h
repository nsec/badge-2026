#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

enum class NfcMode : uint8_t {
  Off,
  Reader,
  Emulator,
};

struct NfcCommand {
  NfcMode mode;
};

extern Queue<NfcCommand> *g_nfcQueue;

}  // namespace core
