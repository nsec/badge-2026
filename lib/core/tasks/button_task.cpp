#include "tasks/button_task.h"

#include <Arduino.h>

#include "hardware/buttons.h"

namespace core {

void ButtonTask::run() {
  bool prevState[hw::BUTTON_COUNT] = {};

  // Read initial state so we don't immediately trigger on held buttons.
  for (uint8_t i = 0; i < hw::BUTTON_COUNT; i++) {
    prevState[i] = hw::buttonPressed(static_cast<hw::Button>(i));
  }

  for (;;) {
    for (uint8_t i = 0; i < hw::BUTTON_COUNT; i++) {
      auto btn = static_cast<hw::Button>(i);
      bool pressed = hw::buttonPressed(btn);

      // Detect rising edge (new press)
      if (pressed && !prevState[i]) {
        _controllerQueue.send(ButtonPressEvent{btn}, core::Milliseconds(0));
        // ^ non-blocking send — drop on full queue rather than stalling input task
      }

      prevState[i] = pressed;
    }

    vTaskDelay(pdMS_TO_TICKS(badge::config::buttons::debounce_delay.count()));
  }
}

}  // namespace core
