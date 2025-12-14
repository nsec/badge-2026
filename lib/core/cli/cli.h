#pragma once

#include <Arduino.h>

namespace core {
namespace cli {

// Command handler function type
typedef void (*CommandHandler)(Stream& stream, const String& args);

// Core CLI functions
void init(Stream &io);
void poll();

// Command registration for modules
void registerCommand(const String& name, const String& help, CommandHandler handler);

} // namespace cli
} // namespace core
