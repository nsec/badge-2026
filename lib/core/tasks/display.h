#pragma once

#include <cstdint>

#include "rtos/queue.hpp"

namespace core {

struct DisplayCommand {
  enum class Type : uint8_t {
    ShowLogo,        // Return to nsec logo (no timeout)
    ModeChange,      // Entering NFC mode (5s timeout)
    NfcScan,         // Card scanned with UID + optional NDEF (15s timeout)
    PairResult,      // Pair outcome (5s timeout)
    SocialProgress,  // Show social category + value (10s timeout)
  };

  Type type = Type::ShowLogo;

  // ModeChange: which NFC mode
  uint8_t nfcMode = 0;  // cast from NfcMode

  // NfcScan: UID string and optional NDEF text
  char uid[32] = {};
  char ndef[64] = {};

  // PairResult: 0=new partner, 1=duplicate, 2=failed
  uint8_t pairOutcome = 0;

  // SocialProgress: which category and current value
  uint8_t socialKey = 0;  // cast from SocialKey
  uint8_t socialValue = 0;
};

extern Queue<DisplayCommand> *g_displayQueue;

}  // namespace core
