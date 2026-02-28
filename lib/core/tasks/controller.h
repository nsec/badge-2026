#pragma once

#include <cstdint>

#include "hardware/buttons.h"
#include "rtos/queue.hpp"

namespace core {

// --- Controller event types ---

enum class EventType : uint8_t {
  ButtonPress,
  ButtonRelease,
  ButtonLongPress,
  CliCommand,
  AppEvent,
};

struct ButtonEventData {
  hw::Button button;
};

struct CliCommandData {
  uint8_t commandId;
  uint8_t args[10];
};

struct AppEventData {
  uint8_t appId;
  uint8_t code;
  uint8_t data[6];
};

struct Event {
  EventType type;
  union {
    ButtonEventData button;
    CliCommandData cli;
    AppEventData app;
  };
};
static_assert(sizeof(Event) <= 16);

extern Queue<Event> *g_controllerQueue;

}  // namespace core
