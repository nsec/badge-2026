#pragma once

#include <Arduino.h>

namespace core {
namespace hw {

// Button identifiers
enum class Button : uint8_t {
  A,
  B,
  Left,
  Right,
  Up,
  Down,
  COUNT  // must be last
};

static constexpr uint8_t BUTTON_COUNT = static_cast<uint8_t>(Button::COUNT);

/**
 * Initialize all button GPIOs with internal pull-ups.
 */
void buttonsInit();

/**
 * Read the current state of a button.
 * Returns true if the button is currently pressed (active low).
 */
bool buttonPressed(Button btn);

/**
 * Get the display name of a button.
 */
const char *buttonName(Button btn);

/**
 * Get the GPIO pin number for a button.
 */
int buttonPin(Button btn);

/**
 * Run interactive button test.
 * Prints to stream when buttons are pressed/released.
 * Exits when all 6 buttons have been pressed, or after timeout_ms.
 * Returns true if all buttons were tested.
 */
bool buttonTestInteractive(Stream &io, uint32_t timeout_ms = 30000);

}  // namespace hw
}  // namespace core
