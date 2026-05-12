#pragma once

#include <Arduino.h>
#include <string>

namespace core {
namespace cli {

// Command handler function type
typedef void (*CommandHandler)(Stream &stream, const std::string &args);

// Core CLI functions
void init(Stream &io);
void poll();

// Command registration for modules
void registerCommand(const std::string &name, const std::string &help, CommandHandler handler);

// Request a fresh prompt on the next poll() cycle (use after async prints).
void requestPrompt();

}  // namespace cli
}  // namespace core
