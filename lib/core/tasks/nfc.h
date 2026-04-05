#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

enum class NfcMode : uint8_t {
  Off,
  Reader,
  Emulator,
  Pair,
};

struct NfcCommand {
  NfcMode mode;
};

extern Queue<NfcCommand> *g_nfcQueue;

// ---------------------------------------------------------------------------
// NDEF emulator text customization
// ---------------------------------------------------------------------------

/// Get the current custom NDEF text (nullptr if using default).
const char *ndefGetText();

/// Set a custom NDEF text (max 100 chars). Returns false on error.
/// Immediately updates the emulated tag memory.
bool ndefSetText(const char *text);

/// Reset NDEF text to the default ("NSEC Badge <MAC>").
void ndefReset();

}  // namespace core
