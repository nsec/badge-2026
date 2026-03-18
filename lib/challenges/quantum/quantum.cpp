#include "quantum.h"
#include <Arduino.h>
#include <string>
#include <cstring>
#include <../core/tasks/cli.h>
#include <../core/hardware/dock.h>
#include <../core/storage/nvs_quantum.h>

namespace challenges {
namespace quantum {

/// Sub-opcodes for the quantum dock challenge (under DockCmd::ChallengeData 0x10).
static constexpr uint8_t QUANTUM_REQUEST = 0x01;
static constexpr uint8_t QUANTUM_RESPONSE = 0x02;

// ---------------------------------------------------------------------------
// Dock handlers
// ---------------------------------------------------------------------------

static void onQuantumRequest(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  uint8_t q1 = core::storage::quantumRead("quantum1");
  uint8_t resp[1] = {q1};
  core::hw::dockSetResponseBuffer(resp, 1);
  Serial.printf("Quantum: dock requested values, sending quantum1=%u\r\n", q1);
}

static void onQuantumResponse(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  if (dataLen < 1)
    return;

  bool success = (data[0] == 0x01);

  if (success && dataLen > 1) {
    char flag[33] = {};
    uint8_t flagLen = dataLen - 1;
    if (flagLen > 32)
      flagLen = 32;
    memcpy(flag, &data[1], flagLen);
    flag[flagLen] = '\0';

    core::storage::quantumWriteStr("flag1", flag);
    Serial.println("Quantum: SUCCESS! Flag stored.");
  } else {
    Serial.println("Quantum: validation failed");
  }
}

// ---------------------------------------------------------------------------
// Init and CLI
// ---------------------------------------------------------------------------

void init() {
  Serial.println("  - Quantum challenge module loaded");

  core::cli::registerCommand("quantum", "quantum challenge (list|set|flag)",
                             [](Stream &stream, const std::string &args) {
                               handleQuantumCommand(stream, args);
                             });

  core::hw::dockRegisterChallengeHandler(QUANTUM_REQUEST, onQuantumRequest);
  core::hw::dockRegisterChallengeHandler(QUANTUM_RESPONSE, onQuantumResponse);
}

void handleQuantumCommand(Stream &stream, const std::string &args) {
  size_t idx = 0;
  std::string sub;

  while (idx < args.size() && args[idx] == ' ')
    idx++;
  size_t start = idx;
  while (idx < args.size() && args[idx] != ' ')
    idx++;
  sub = args.substr(start, idx - start);

  if (sub.empty() || sub == "help") {
    stream.println("=== Quantum Challenge ===");
    stream.println();
    stream.println("Usage:");
    stream.println("  quantum list            - show quantum NVS values");
    stream.println("  quantum set <key> <val> - set quantum1 (0-255)");
    stream.println("  quantum flag            - show captured flag");
    stream.println();
    stream.println("Insert your badge into a quantum dock station");
    stream.println("to submit your values for validation.");
    return;
  }

  if (sub == "list") {
    uint8_t q1 = core::storage::quantumRead("quantum1");
    stream.printf("  quantum1 = %u\r\n", q1);
    return;
  }

  if (sub == "flag") {
    char flag[33] = {};
    if (core::storage::quantumReadStr("flag1", flag, sizeof(flag))) {
      stream.printf("  flag1 = %s\r\n", flag);
    } else {
      stream.println("  flag1 = (not yet captured)");
    }
    return;
  }

  if (sub == "set") {
    while (idx < args.size() && args[idx] == ' ')
      idx++;
    size_t ks = idx;
    while (idx < args.size() && args[idx] != ' ')
      idx++;
    std::string key = args.substr(ks, idx - ks);

    while (idx < args.size() && args[idx] == ' ')
      idx++;
    size_t vs = idx;
    while (idx < args.size() && args[idx] != ' ')
      idx++;
    std::string valStr = args.substr(vs, idx - vs);

    if (key != "quantum1") {
      stream.println("Unknown key. Use: quantum1");
      return;
    }
    if (valStr.empty()) {
      stream.println("Usage: quantum set quantum1 <0-255>");
      return;
    }

    int v = atoi(valStr.c_str());
    if (v < 0 || v > 255) {
      stream.println("Value must be 0-255.");
      return;
    }

    core::storage::quantumWrite(key.c_str(), static_cast<uint8_t>(v));
    stream.printf("Set %s = %u\r\n", key.c_str(), v);
    return;
  }

  stream.println("Unknown sub-command. Type 'quantum help' for usage.");
}

}  // namespace quantum
}  // namespace challenges
