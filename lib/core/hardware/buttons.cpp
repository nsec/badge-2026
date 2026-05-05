#include "buttons.h"
#include "board_pins.h"

#include <Arduino.h>

namespace core {
namespace hw {

struct ButtonDef {
  Button id;
  int pin;
  const char *name;
};

static const ButtonDef g_buttons[] = {
    {Button::A, badge::pins::BTN_A, "A"},          {Button::B, badge::pins::BTN_B, "B"},
    {Button::Left, badge::pins::BTN_LEFT, "Left"}, {Button::Right, badge::pins::BTN_RIGHT, "Right"},
    {Button::Up, badge::pins::BTN_UP, "Up"},       {Button::Down, badge::pins::BTN_DOWN, "Down"},
};

void buttonsInit() {
  for (const auto &b : g_buttons) {
    pinMode(b.pin, INPUT_PULLUP);
  }
  // Serial.println("Buttons initialized (6 buttons, active-low with pull-up)");
}

bool buttonPressed(Button btn) {
  uint8_t idx = static_cast<uint8_t>(btn);
  if (idx >= BUTTON_COUNT)
    return false;
  return digitalRead(g_buttons[idx].pin) == LOW;
}

const char *buttonName(Button btn) {
  uint8_t idx = static_cast<uint8_t>(btn);
  if (idx >= BUTTON_COUNT)
    return "?";
  return g_buttons[idx].name;
}

int buttonPin(Button btn) {
  uint8_t idx = static_cast<uint8_t>(btn);
  if (idx >= BUTTON_COUNT)
    return -1;
  return g_buttons[idx].pin;
}

bool buttonTestInteractive(Stream &io, uint32_t timeout_ms) {
  io.println("=== Button Test ===");
  io.println("Press each button to test. Exits when all tested or timeout.");
  io.println();

  // Print button map
  for (const auto &b : g_buttons) {
    io.print("  ");
    io.print(b.name);
    io.print(" -> IO");
    io.println(b.pin);
  }
  io.println();
  io.println("Waiting for button presses...");
  io.println("(Press Ctrl+C or wait 30s to exit early)");
  io.println();

  bool tested[BUTTON_COUNT] = {};
  bool prevState[BUTTON_COUNT] = {};
  int testedCount = 0;

  // Read initial state so we don't trigger on already-held buttons
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    prevState[i] = (digitalRead(g_buttons[i].pin) == LOW);
  }

  uint32_t startTime = millis();

  while (testedCount < BUTTON_COUNT) {
    // Check timeout
    if (millis() - startTime > timeout_ms) {
      io.println();
      io.println("Timeout reached.");
      break;
    }

    // Check for Ctrl+C
    if (io.available() > 0) {
      char c = (char)io.read();
      if (c == 0x03) {  // Ctrl+C
        io.println();
        io.println("Cancelled.");
        break;
      }
    }

    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      bool pressed = (digitalRead(g_buttons[i].pin) == LOW);

      if (pressed && !prevState[i]) {
        // Button just pressed
        io.print("  [PRESS]   ");
        io.print(g_buttons[i].name);
        io.print(" (IO");
        io.print(g_buttons[i].pin);
        io.print(")");

        if (!tested[i]) {
          tested[i] = true;
          testedCount++;
          io.print("  ✓ OK");
        }
        io.println();
      } else if (!pressed && prevState[i]) {
        // Button just released
        io.print("  [RELEASE] ");
        io.println(g_buttons[i].name);
      }

      prevState[i] = pressed;
    }

    delay(20);  // debounce
  }

  // Summary
  io.println();
  io.println("--- Results ---");
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    io.print("  ");
    io.print(g_buttons[i].name);
    for (int pad = strlen(g_buttons[i].name); pad < 8; pad++)
      io.print(" ");
    io.print("IO");
    io.print(g_buttons[i].pin);
    io.print("  ");
    io.println(tested[i] ? "PASS" : "NOT TESTED");
  }

  io.print("\n");
  io.print(testedCount);
  io.print("/");
  io.print(BUTTON_COUNT);
  io.println(" buttons verified.");

  return testedCount == BUTTON_COUNT;
}

}  // namespace hw
}  // namespace core
