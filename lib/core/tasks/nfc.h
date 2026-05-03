#pragma once

#include <cstdint>

#include <badge_config.h>

#include "rtos/queue.hpp"

namespace core {

enum class NfcMode : uint8_t {
  Off,
  Reader,
  Emulator,
  WifiEmulator,
  UrlEmulator,
  Pair,
};

struct NfcCommand {
  NfcMode mode;

  union {
    struct {
      char ssid[badge::config::wifi::ssid_max_len + 1];
      char passphrase[badge::config::wifi::passphrase_len + 1];
    } wifi;
  };
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
