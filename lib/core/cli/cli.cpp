#include "cli.h"

#include <Arduino.h>
#include <vector>

#include "../system/ota_manager.h"

namespace {
Stream *g_io = nullptr;
String g_line;

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
  g_io->println("  boot conference      - set next boot to conference partition (then reboot)");
  g_io->println("  boot ctf             - set next boot to CTF partition (then reboot)");
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

void cmdReboot() {
  g_io->println("Rebooting...");
  delay(50);
  ESP.restart();
}

void cmdBoot(const String &arg) {
  if (arg == "conference") {
    if (core::ota::setNextBoot(core::ota::BootTarget::Conference, *g_io)) cmdReboot();
    return;
  }
  if (arg == "ctf") {
    if (core::ota::setNextBoot(core::ota::BootTarget::Ctf, *g_io)) cmdReboot();
    return;
  }

  g_io->println("Usage: boot conference|ctf");
}

void handleLine(const String &line) {
  int i = 0;
  String cmd = nextToken(line, i);
  cmd.toLowerCase();

  if (cmd.length() == 0) return;

  // Built-in commands
  if (cmd == "help" || cmd == "?") return cmdHelp();
  if (cmd == "info") return cmdInfo();
  if (cmd == "reboot") return cmdReboot();

  if (cmd == "boot") {
    String arg = nextToken(line, i);
    arg.toLowerCase();
    return cmdBoot(arg);
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
  prompt();
}

void poll() {
  if (!g_io) return;

  while (g_io->available() > 0) {
    const char c = (char)g_io->read();

    // Handle both \r and \n as line endings (but avoid duplicate processing on \r\n)
    if (c == '\r' || c == '\n') {
      if (g_line.length() > 0) {
        g_io->println();
        handleLine(g_line);
        g_line = "";
        prompt();
      }
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
