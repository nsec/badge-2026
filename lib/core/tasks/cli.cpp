#include "tasks/cli.h"

#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "system/ota_manager.h"
#include "hardware/hwid.h"
#include "hardware/buttons.h"
#include "tasks/led.h"
#include "tasks/controller.h"
#include "tasks/cli_queue.h"
#include "storage/nvs_social.h"
#include "tasks/nfc.h"

namespace {

// Helper functions for std::string
inline void toLower(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
}

inline void trim(std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  size_t end = s.find_last_not_of(" \t\r\n");
  if (start == std::string::npos) {
    s.clear();
  } else {
    s = s.substr(start, end - start + 1);
  }
}

Stream *g_io = nullptr;
std::string g_line;
bool g_promptNeeded = true;  // deferred prompt flag

// Command history
static constexpr int HISTORY_SIZE = 16;
std::string g_history[HISTORY_SIZE];
int g_historyCount = 0;   // total items stored
int g_historyIdx = -1;    // current browse position (-1 = not browsing)
std::string g_savedLine;  // line saved when user starts browsing

// ANSI escape sequence state machine
enum class EscState { None, GotEsc, GotBracket };
EscState g_escState = EscState::None;

// Registered commands
struct Command {
  std::string name;
  std::string help;
  core::cli::CommandHandler handler;
};

std::vector<Command> g_commands;

void prompt() {
  if (!g_io)
    return;
  g_io->print("> ");
  g_io->flush();
}

// Clear the current line on the terminal and replace with new text
void replaceLine(const std::string &newLine) {
  if (!g_io)
    return;
  // Erase current display: move cursor to start of input, overwrite with spaces, move back
  for (size_t i = g_line.length(); i > 0; i--) {
    g_io->print("\b \b");
  }
  g_line = newLine;
  g_io->print(g_line.c_str());
}

void historyAdd(const std::string &line) {
  if (line.length() == 0)
    return;
  // Don't add duplicates of the most recent entry
  if (g_historyCount > 0 && g_history[(g_historyCount - 1) % HISTORY_SIZE] == line)
    return;
  g_history[g_historyCount % HISTORY_SIZE] = line;
  g_historyCount++;
}

void historyBrowseUp() {
  if (g_historyCount == 0)
    return;
  if (g_historyIdx == -1) {
    // Starting to browse - save current input
    g_savedLine = g_line;
    g_historyIdx = g_historyCount - 1;
  } else if (g_historyIdx > 0 && g_historyIdx > g_historyCount - HISTORY_SIZE) {
    g_historyIdx--;
  } else {
    return;  // at oldest entry
  }
  replaceLine(g_history[g_historyIdx % HISTORY_SIZE]);
}

void historyBrowseDown() {
  if (g_historyIdx == -1)
    return;
  g_historyIdx++;
  if (g_historyIdx >= g_historyCount) {
    // Back to current input
    g_historyIdx = -1;
    replaceLine(g_savedLine);
  } else {
    replaceLine(g_history[g_historyIdx % HISTORY_SIZE]);
  }
}

std::string nextToken(const std::string &s, size_t &idx) {
  while (idx < s.length() && std::isspace(static_cast<unsigned char>(s[idx])))
    idx++;
  size_t start = idx;
  while (idx < s.length() && !std::isspace(static_cast<unsigned char>(s[idx])))
    idx++;
  if (start == idx)
    return std::string();
  return s.substr(start, idx - start);
}

void cmdHelp() {
  g_io->println("Commands:");
  g_io->println("  help                 - show this help");
  g_io->println("  info                 - print current boot/partition info");
  g_io->println("  hwid                 - print unique hardware ID");
  g_io->println("  ledtest [N]          - run RGB LED test suite (N=test# or all)");
  g_io->println("  buttontest           - interactive button test (press all 6)");
  g_io->println("  nvstest <key> <val>  - set social NVS");
  g_io->println("  pairtest [reset]     - show/reset paired partners");
  g_io->println("  status               - show social NVS values");
  g_io->println("  clear                - clear the screen");
  g_io->println("  swapboot             - switch to other firmware and reboot");
  g_io->println("  reboot               - reboot now");

  // Show registered module commands
  for (const auto &cmd : g_commands) {
    g_io->print("  ");
    g_io->print(cmd.name.c_str());
    // Pad to align help text
    for (size_t i = cmd.name.length(); i < 20; i++)
      g_io->print(" ");
    g_io->print(" - ");
    g_io->println(cmd.help.c_str());
  }
}

void cmdInfo() {
  core::ota::printBootInfo(*g_io);
}

void cmdHwid() {
  core::hw::printHardwareId(*g_io);
}

const char *animationName(core::LedCommandType type) {
  switch (type) {
    case core::LedCommandType::SolidRed:
      return "All RED";
    case core::LedCommandType::SolidGreen:
      return "All GREEN";
    case core::LedCommandType::SolidBlue:
      return "All BLUE";
    case core::LedCommandType::SolidWhite:
      return "All WHITE";
    case core::LedCommandType::SolidOrange:
      return "All ORANGE";
    case core::LedCommandType::SolidCyan:
      return "All CYAN";
    case core::LedCommandType::PixelWalk:
      return "Pixel walk";
    case core::LedCommandType::Rainbow:
      return "Rainbow";
    case core::LedCommandType::Off:
      return "All OFF";
    case core::LedCommandType::ProgressFlash:
      return "Progress flash";
  }
  return "Unknown";
}

void ledTestProgress(uint8_t step, uint8_t total, core::LedCommandType anim) {
  g_io->printf("  [%d/%d] %s\n\r", step, total, animationName(anim));
}

void cmdLedTest(const std::string &arg) {
  g_io->println("=== RGB LED Test Suite ===");
  g_io->println();

  int testNum = arg.empty() ? 0 : std::atoi(arg.c_str());
  if (testNum < 0 || testNum > 7) {
    g_io->println("Usage: ledtest [1-7]  (omit number to run all)");
    return;
  }

  core::LedTestRequest req;
  req.progress = ledTestProgress;
  if (testNum > 0)
    req.testNum = testNum;

  // Request LED test from the controller, wait for completion.
  core::g_controllerQueue->send(req);

  core::CliResponse response;
  core::g_cliQueue->receive(response);

  g_io->println();
  g_io->println("LED test complete.");
}

void cmdStatus() {
  g_io->println("=== Social Status ===");
  const core::storage::SocialKey keys[] = {
      core::storage::SocialKey::Social,
      core::storage::SocialKey::Sponsor,
      core::storage::SocialKey::Light,
      core::storage::SocialKey::Attraction,
  };
  for (auto k : keys) {
    uint8_t val = core::storage::socialRead(k);
    g_io->printf("  %-12s = %u\r\n", core::storage::socialKeyName(k), val);
  }
}

void cmdNvsTest(const std::string &args) {
  // Parse: nvstest <social|sponsor|light|attraction> <0-255>
  size_t idx = 0;
  std::string keyStr = nextToken(args, idx);
  std::string valStr = nextToken(args, idx);
  toLower(keyStr);

  if (keyStr.empty() || valStr.empty()) {
    cmdStatus();
    return;
  }

  core::storage::SocialKey key;
  bool setAll = false;
  if (keyStr == "social")
    key = core::storage::SocialKey::Social;
  else if (keyStr == "sponsor")
    key = core::storage::SocialKey::Sponsor;
  else if (keyStr == "light")
    key = core::storage::SocialKey::Light;
  else if (keyStr == "attraction")
    key = core::storage::SocialKey::Attraction;
  else if (keyStr == "all")
    setAll = true;
  else {
    g_io->println("Unknown key. Use: social, sponsor, light, attraction, all");
    return;
  }

  int v = std::atoi(valStr.c_str());
  if (v < 0 || v > 255) {
    g_io->println("Value must be 0-255.");
    return;
  }

  uint8_t value = static_cast<uint8_t>(v);

  if (setAll) {
    const core::storage::SocialKey allKeys[] = {
        core::storage::SocialKey::Social,
        core::storage::SocialKey::Sponsor,
        core::storage::SocialKey::Light,
        core::storage::SocialKey::Attraction,
    };
    for (auto k : allKeys) {
      core::SocialSetRequest req{k, value};
      core::g_controllerQueue->send(req);
      core::CliResponse response;
      core::g_cliQueue->receive(response);
    }
    g_io->printf("NVS all keys set to %u\r\n", value);
    return;
  }

  // Send to controller task (which handles the NVS write in its own context)
  core::SocialSetRequest req{key, value};
  core::g_controllerQueue->send(req);

  // Wait for confirmation
  core::CliResponse response;
  core::g_cliQueue->receive(response);

  // Read back to confirm
  uint8_t readback = core::storage::socialRead(key);
  g_io->printf("NVS '%s' set to %u (readback: %u)\r\n", core::storage::socialKeyName(key), value, readback);
}

void cmdPairTest(const std::string &args) {
  std::string arg = args;
  toLower(arg);
  trim(arg);

  if (arg == "reset") {
    core::storage::pairReset();
    core::storage::socialWrite(core::storage::SocialKey::Social, 0);
    g_io->println("Paired partners and social value reset to 0");
    return;
  }

  uint16_t count = core::storage::pairCount();
  g_io->printf("Paired partners: %d\r\n", count);

  for (uint16_t i = 0; i < count; i++) {
    uint8_t mac[6];
    if (core::storage::pairGet(i, mac)) {
      g_io->printf("  %3d: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   i + 1, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
  }

  g_io->printf("Social value: %d\r\n", core::storage::socialRead(core::storage::SocialKey::Social));
}

void cmdReboot() {
  g_io->println("Rebooting...");
  delay(50);
  ESP.restart();
}

void cmdBoot() {
  std::string current(core::ota::getRunningPartitionLabel().c_str());
  core::ota::BootTarget target;
  const char *targetName;

  if (current == "ctf") {
    target = core::ota::BootTarget::Conference;
    targetName = "conference";
  } else {
    target = core::ota::BootTarget::Ctf;
    targetName = "ctf";
  }

  g_io->print("Currently on: ");
  g_io->println(current.c_str());
  g_io->print("Switching to: ");
  g_io->println(targetName);

  if (core::ota::setNextBoot(target, *g_io))
    cmdReboot();
}

void handleLine(const std::string &line) {
  size_t i = 0;
  std::string cmd = nextToken(line, i);
  toLower(cmd);

  if (cmd.length() == 0)
    return;

  // These commands are available through both firmware versions, and include test
  // commands meant to facilitate development. They should be commented out/removed
  // before the final release.

  // Built-in commands
  if (cmd == "help" || cmd == "?")
    return cmdHelp();
  if (cmd == "info")
    return cmdInfo();
  if (cmd == "hwid")
    return cmdHwid();
  if (cmd == "ledtest") {
    std::string arg = nextToken(line, i);
    return cmdLedTest(arg);
  }
  if (cmd == "buttontest") {
    core::hw::buttonTestInteractive(*g_io);
    return;
  }
  if (cmd == "nvstest") {
    std::string arg = (i < line.length()) ? line.substr(i) : "";
    trim(arg);
    return cmdNvsTest(arg);
  }  if (cmd == "pairtest") {
    std::string arg = (i < line.length()) ? line.substr(i) : "";
    trim(arg);
    return cmdPairTest(arg);
  }  if (cmd == "status")
    return cmdStatus();
  if (cmd == "clear") {
    g_io->print("\033[2J\033[H");
    return;
  }
  if (cmd == "reboot")
    return cmdReboot();

  if (cmd == "swapboot") {
    return cmdBoot();
  }

  // Check registered module commands
  for (const auto &registeredCmd : g_commands) {
    if (cmd == registeredCmd.name) {
      // Get remaining arguments
      std::string args = (i < line.length()) ? line.substr(i) : "";
      trim(args);
      registeredCmd.handler(*g_io, args);
      return;
    }
  }

  g_io->print("Unknown command: ");
  g_io->println(cmd.c_str());
  g_io->println("Type 'help' for commands.");
}
}  // namespace

namespace core {
namespace cli {

void init(Stream &io) {
  g_io = &io;
  g_line.reserve(128);
  // Don't print prompt here - main.cpp still has boot messages to print.
  // Set flag so poll() prints it once everything is ready.
  g_promptNeeded = true;
}

void poll() {
  if (!g_io)
    return;

  // Deferred prompt: print once when poll is first called (after all boot messages)
  if (g_promptNeeded) {
    g_promptNeeded = false;
    prompt();
  }

  while (g_io->available() > 0) {
    const char c = (char)g_io->read();

    // ANSI escape sequence handling (arrow keys send ESC [ A/B/C/D)
    if (g_escState == EscState::GotEsc) {
      if (c == '[') {
        g_escState = EscState::GotBracket;
        continue;
      }
      g_escState = EscState::None;
      // Not an escape sequence, fall through
    } else if (g_escState == EscState::GotBracket) {
      g_escState = EscState::None;
      if (c == 'A') {
        historyBrowseUp();
        continue;
      }  // Up arrow
      if (c == 'B') {
        historyBrowseDown();
        continue;
      }  // Down arrow
      // C = Right, D = Left — ignore for now
      continue;
    }

    if (c == 0x1B) {  // ESC
      g_escState = EscState::GotEsc;
      continue;
    }

    // Handle both \r and \n as line endings (but avoid duplicate processing on \r\n)
    if (c == '\r' || c == '\n') {
      g_io->println();
      if (g_line.length() > 0) {
        historyAdd(g_line);
        handleLine(g_line);
        g_line.clear();
      }
      g_historyIdx = -1;  // reset history browsing
      prompt();
      continue;
    }

    // Backspace
    if (c == 0x08 || c == 0x7F) {
      if (g_line.length() > 0) {
        g_line.pop_back();
        g_io->print("\b \b");
      }
      continue;
    }

    // Ctrl+C - cancel current line
    if (c == 0x03) {
      g_io->println("^C");
      g_line.clear();
      g_historyIdx = -1;
      prompt();
      continue;
    }

    if (isPrintable((unsigned char)c)) {
      if (g_line.length() < 127) {
        g_line += c;
        g_io->print(c);
      }
    }
  }
}

void registerCommand(const std::string &name, const std::string &help, CommandHandler handler) {
  Command cmd;
  cmd.name = name;
  cmd.help = help;
  cmd.handler = handler;
  g_commands.push_back(cmd);
}

}  // namespace cli
}  // namespace core
