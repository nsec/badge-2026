#include "cli.h"

#include <Arduino.h>
#include <vector>

#include "../system/ota_manager.h"
#include "../hardware/hwid.h"
#include "../hardware/rgb_led.h"
#include "../hardware/buttons.h"

namespace {
Stream *g_io = nullptr;
String g_line;
bool g_promptNeeded = true;  // deferred prompt flag

// Command history
static constexpr int HISTORY_SIZE = 16;
String g_history[HISTORY_SIZE];
int g_historyCount = 0;   // total items stored
int g_historyIdx = -1;    // current browse position (-1 = not browsing)
String g_savedLine;        // line saved when user starts browsing

// ANSI escape sequence state machine
enum class EscState { None, GotEsc, GotBracket };
EscState g_escState = EscState::None;

// Registered commands
struct Command {
  String name;
  String help;
  core::cli::CommandHandler handler;
};
std::vector<Command> g_commands;

void prompt() {
  if (!g_io) return;
  g_io->print("> ");
  g_io->flush();
}

// Clear the current line on the terminal and replace with new text
void replaceLine(const String &newLine) {
  if (!g_io) return;
  // Erase current display: move cursor to start of input, overwrite with spaces, move back
  for (int i = g_line.length(); i > 0; i--) {
    g_io->print("\b \b");
  }
  g_line = newLine;
  g_io->print(g_line);
}

void historyAdd(const String &line) {
  if (line.length() == 0) return;
  // Don't add duplicates of the most recent entry
  if (g_historyCount > 0 && g_history[(g_historyCount - 1) % HISTORY_SIZE] == line) return;
  g_history[g_historyCount % HISTORY_SIZE] = line;
  g_historyCount++;
}

void historyBrowseUp() {
  if (g_historyCount == 0) return;
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
  if (g_historyIdx == -1) return;
  g_historyIdx++;
  if (g_historyIdx >= g_historyCount) {
    // Back to current input
    g_historyIdx = -1;
    replaceLine(g_savedLine);
  } else {
    replaceLine(g_history[g_historyIdx % HISTORY_SIZE]);
  }
}

String nextToken(const String &s, int &idx) {
  while (idx < (int)s.length() && isspace((unsigned char)s[idx])) idx++;
  int start = idx;
  while (idx < (int)s.length() && !isspace((unsigned char)s[idx])) idx++;
  if (start == idx) return String();
  return s.substring(start, idx);
}

void cmdHelp() {
  g_io->println("Commands:");
  g_io->println("  help                 - show this help");
  g_io->println("  info                 - print current boot/partition info");
  g_io->println("  hwid                 - print unique hardware ID");
  g_io->println("  ledtest [N]          - run RGB LED test suite (N=test# or all)");
  g_io->println("  buttontest           - interactive button test (press all 6)");
  g_io->println("  swapboot             - switch to other firmware and reboot");
  g_io->println("  reboot               - reboot now");
  
  // Show registered module commands
  for (const auto& cmd : g_commands) {
    g_io->print("  ");
    g_io->print(cmd.name);
    // Pad to align help text
    for (int i = cmd.name.length(); i < 20; i++) g_io->print(" ");
    g_io->print(" - ");
    g_io->println(cmd.help);
  }
}

void cmdInfo() {
  core::ota::printBootInfo(*g_io);
}

void cmdHwid() {
  core::hw::printHardwareId(*g_io);
}

void ledTestRed() {
  g_io->println("  [1/7] All RED");
  core::hw::rgbSetAll(255, 0, 0);
  delay(1000);
}

void ledTestGreen() {
  g_io->println("  [2/7] All GREEN");
  core::hw::rgbSetAll(0, 255, 0);
  delay(1000);
}

void ledTestBlue() {
  g_io->println("  [3/7] All BLUE");
  core::hw::rgbSetAll(0, 0, 255);
  delay(1000);
}

void ledTestWhite() {
  g_io->println("  [4/7] All WHITE");
  core::hw::rgbSetAll(255, 255, 255);
  delay(1000);
}

void ledTestChase() {
  g_io->println("  [5/7] Chase (pixel walk)");
  for (uint8_t i = 0; i < core::hw::RGB_LED_COUNT; i++) {
    core::hw::rgbClear();
    core::hw::rgbSetPixel(i, 255, 0, 255);
    core::hw::rgbShow();
    delay(100);
  }
  delay(500);
}

void ledTestRainbow() {
  g_io->println("  [6/7] Rainbow");
  for (int frame = 0; frame < 256; frame += 4) {
    for (uint8_t i = 0; i < core::hw::RGB_LED_COUNT; i++) {
      uint8_t hue = (frame + i * 256 / core::hw::RGB_LED_COUNT) & 0xFF;
      // Simple HSV-to-RGB (hue only, full sat/val)
      uint8_t r, g, b;
      uint8_t region = hue / 43;
      uint8_t remainder = (hue - region * 43) * 6;
      switch (region) {
        case 0:  r = 255; g = remainder; b = 0; break;
        case 1:  r = 255 - remainder; g = 255; b = 0; break;
        case 2:  r = 0; g = 255; b = remainder; break;
        case 3:  r = 0; g = 255 - remainder; b = 255; break;
        case 4:  r = remainder; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - remainder; break;
      }
      core::hw::rgbSetPixel(i, r, g, b);
    }
    core::hw::rgbShow();
    delay(20);
  }
  delay(500);
}

void ledTestOff() {
  g_io->println("  [7/7] All OFF");
  core::hw::rgbClear();
  delay(500);
}

void cmdLedTest(const String &arg) {
  g_io->println("=== RGB LED Test Suite ===");
  g_io->print("LEDs: ");
  g_io->print(core::hw::RGB_LED_COUNT);
  g_io->println(" on IO8");
  g_io->println();

  int testNum = arg.toInt();  // 0 if empty/invalid = run all

  if (testNum == 0 || testNum == 1) ledTestRed();
  if (testNum == 0 || testNum == 2) ledTestGreen();
  if (testNum == 0 || testNum == 3) ledTestBlue();
  if (testNum == 0 || testNum == 4) ledTestWhite();
  if (testNum == 0 || testNum == 5) ledTestChase();
  if (testNum == 0 || testNum == 6) ledTestRainbow();
  if (testNum == 0 || testNum == 7) ledTestOff();

  if (testNum < 0 || testNum > 7) {
    g_io->println("Usage: ledtest [1-7]  (omit number to run all)");
    g_io->println("  1=Red  2=Green  3=Blue  4=White");
    g_io->println("  5=Chase  6=Rainbow  7=Off");
    return;
  }

  g_io->println();
  g_io->println("LED test complete.");
}

void cmdReboot() {
  g_io->println("Rebooting...");
  delay(50);
  ESP.restart();
}

void cmdBoot() {
  String current = core::ota::getRunningPartitionLabel();
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
  g_io->println(current);
  g_io->print("Switching to: ");
  g_io->println(targetName);

  if (core::ota::setNextBoot(target, *g_io)) cmdReboot();
}

void handleLine(const String &line) {
  int i = 0;
  String cmd = nextToken(line, i);
  cmd.toLowerCase();

  if (cmd.length() == 0) return;

  // Built-in commands
  if (cmd == "help" || cmd == "?") return cmdHelp();
  if (cmd == "info") return cmdInfo();
  if (cmd == "hwid") return cmdHwid();
  if (cmd == "ledtest") {
    String arg = nextToken(line, i);
    return cmdLedTest(arg);
  }
  if (cmd == "buttontest") {
    core::hw::buttonTestInteractive(*g_io);
    return;
  }
  if (cmd == "reboot") return cmdReboot();

  if (cmd == "swapboot") {
    return cmdBoot();
  }

  // Check registered module commands
  for (const auto& registeredCmd : g_commands) {
    if (cmd == registeredCmd.name) {
      // Get remaining arguments
      String args = line.substring(i);
      args.trim();
      registeredCmd.handler(*g_io, args);
      return;
    }
  }

  g_io->print("Unknown command: ");
  g_io->println(cmd);
  g_io->println("Type 'help' for commands.");
}
} // namespace

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
  if (!g_io) return;

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
      if (c == 'A') { historyBrowseUp(); continue; }   // Up arrow
      if (c == 'B') { historyBrowseDown(); continue; }  // Down arrow
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
        g_line = "";
      }
      g_historyIdx = -1;  // reset history browsing
      prompt();
      continue;
    }

    // Backspace
    if (c == 0x08 || c == 0x7F) {
      if (g_line.length() > 0) {
        g_line.remove(g_line.length() - 1);
        g_io->print("\b \b");
      }
      continue;
    }

    // Ctrl+C - cancel current line
    if (c == 0x03) {
      g_io->println("^C");
      g_line = "";
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

void registerCommand(const String& name, const String& help, CommandHandler handler) {
  Command cmd;
  cmd.name = name;
  cmd.help = help;
  cmd.handler = handler;
  g_commands.push_back(cmd);
}

} // namespace cli
} // namespace core
