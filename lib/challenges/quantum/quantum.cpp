#include "quantum.h"
#include "crystal.h"
#include "grid.h"
#include <Arduino.h>
#include <string>
#include <cstring>
#include <../core/tasks/cli.h>
#include <../core/hardware/dock.h>
#include <../core/storage/nvs_quantum.h>

namespace challenges {
namespace quantum {

/// Sub-opcodes for the quantum dock challenge (under DockCmd::ChallengeData 0x10).
static constexpr uint8_t CRYSTAL_REQUEST = 0x03;  // dock reads CrystalState blob
static constexpr uint8_t CRYSTAL_RESULT = 0x05;   // dock sends crystal validation + flag
static constexpr uint8_t GRID_REQUEST = 0x04;     // dock reads GridState blob
static constexpr uint8_t GRID_RESULT = 0x06;      // dock sends grid validation + flag

// ---------------------------------------------------------------------------
// Dock handlers
// ---------------------------------------------------------------------------

// Crystal dock handler — serves the CrystalState blob for dock to read
static void onCrystalRequest(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  challenges::crystal::CrystalState st;
  if (challenges::crystal::load(st)) {
    core::hw::dockSetResponseBuffer(reinterpret_cast<const uint8_t *>(&st), sizeof(st));
    Serial.printf("Quantum: serving CrystalState (%d bytes, solved=%d)\r\n", sizeof(st), st.solved);
  } else {
    // No valid state — send a zeroed response
    uint8_t empty[1] = {0};
    core::hw::dockSetResponseBuffer(empty, 1);
    Serial.println("Quantum: no crystal state for dock");
  }
}

// Crystal result handler — dock sends validation result + flag
static void onCrystalResult(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  if (dataLen < 1)
    return;

  bool success = (data[0] == 0x01);

  if (success && dataLen > 1) {
    char flag[49] = {};
    uint8_t flagLen = dataLen - 1;
    if (flagLen > 48)
      flagLen = 48;
    memcpy(flag, &data[1], flagLen);
    flag[flagLen] = '\0';

    core::storage::quantumWriteStr("cflag", flag);
    Serial.println("Crystal: SUCCESS! Flag stored.");
  } else {
    Serial.println("Crystal: dock validation failed");
  }
}

// Grid dock handler — serves the GridState blob for dock to read
static void onGridRequest(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  challenges::grid::GridState st;
  if (challenges::grid::load(st)) {
    core::hw::dockSetResponseBuffer(reinterpret_cast<const uint8_t *>(&st), sizeof(st));
    Serial.printf("Quantum: serving GridState (%d bytes, solved=%d)\r\n", sizeof(st), st.solved);
  } else {
    uint8_t empty[1] = {0};
    core::hw::dockSetResponseBuffer(empty, 1);
    Serial.println("Quantum: no grid state for dock");
  }
}

// Grid result handler — dock sends validation result + flag
static void onGridResult(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen) {
  if (dataLen < 1)
    return;

  bool success = (data[0] == 0x01);
  if (success && dataLen > 1) {
    char flag[49] = {};
    uint8_t flagLen = dataLen - 1;
    if (flagLen > 48)
      flagLen = 48;
    memcpy(flag, &data[1], flagLen);
    flag[flagLen] = '\0';
    core::storage::quantumWriteStr("gflag", flag);
    Serial.println("Grid: SUCCESS! Flag stored.");
  } else {
    Serial.println("Grid: dock validation failed");
  }
}

// ---------------------------------------------------------------------------
// Init and CLI
// ---------------------------------------------------------------------------

void init() {
  Serial.println("  - Quantum challenge module loaded");

  core::cli::registerCommand("quantum", "quantum challenge (crystal|grid|flag)",
                             [](Stream &stream, const std::string &args) {
                               handleQuantumCommand(stream, args);
                             });

  core::hw::dockRegisterChallengeHandler(CRYSTAL_REQUEST, onCrystalRequest);
  core::hw::dockRegisterChallengeHandler(CRYSTAL_RESULT, onCrystalResult);
  core::hw::dockRegisterChallengeHandler(GRID_REQUEST, onGridRequest);
  core::hw::dockRegisterChallengeHandler(GRID_RESULT, onGridResult);
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
    stream.print("=== Quantum Challenge Track ===\r\n"
                 "\r\n"
                 "Usage:\r\n"
                 "  quantum crystal <cmd>   - Crystal Tuning (VQE) challenge\r\n"
                 "  quantum grid <cmd>      - Grid Optimization (QAOA) challenge\r\n"
                 "  quantum flag            - show captured flags\r\n"
                 "\r\n"
                 "Crystal sub-commands:\r\n"
                 "  crystal info            - show Hamiltonian & ansatz\r\n"
                 "  crystal set <idx> <val> - set one working param\r\n"
                 "  crystal params          - show working params & energy\r\n"
                 "  crystal run             - evaluate (uses working params)\r\n"
                 "  crystal sweep <idx> <lo> <hi> <steps>\r\n"
                 "                          - sweep one param, auto-saves best\r\n"
                 "  crystal store           - store working params to NVS\r\n"
                 "  crystal status / reset\r\n"
                 "\r\n"
                 "Grid sub-commands:\r\n"
                 "  grid info               - show cost Hamiltonian\r\n"
                 "  grid run <g1 g2 b1 b2>  - evaluate QAOA\r\n"
                 "  grid hist <g1 g2 b1 b2> - energy histogram\r\n"
                 "  grid store <g1 g2 b1 b2> - store result to NVS\r\n"
                 "  grid status / reset\r\n");
    stream.flush();
    return;
  }

  // --- crystal sub-commands ---
  if (sub == "crystal") {
    std::string rest = (idx < args.size()) ? args.substr(idx) : "";
    challenges::crystal::handleCommand(stream, rest);
    return;
  }

  // --- grid sub-commands ---
  if (sub == "grid") {
    std::string rest = (idx < args.size()) ? args.substr(idx) : "";
    challenges::grid::handleCommand(stream, rest);
    return;
  }

  if (sub == "flag") {
    char cflag[49] = {};
    if (core::storage::quantumReadStr("cflag", cflag, sizeof(cflag))) {
      stream.printf("  crystal flag = %s\r\n", cflag);
    } else {
      stream.println("  crystal flag = (not yet captured)");
    }
    char gflag[49] = {};
    if (core::storage::quantumReadStr("gflag", gflag, sizeof(gflag))) {
      stream.printf("  grid flag = %s\r\n", gflag);
    } else {
      stream.println("  grid flag = (not yet captured)");
    }
    return;
  }

  stream.println("Unknown sub-command. Type 'quantum help' for usage.");
}

}  // namespace quantum
}  // namespace challenges
