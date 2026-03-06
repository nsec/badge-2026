#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/controller.h"
#include "tasks/led.h"
#include "tasks/cli_queue.h"

namespace core {

class ControllerTask : public Task {
public:
  ControllerTask(Queue<ControllerEvent> &inQueue, Queue<LedCommand> &ledQueue, Queue<CliResponse> &cliQueue)
      : Task("controller", badge::config::tasks::priority_controller), _inQueue(inQueue), _ledQueue(ledQueue),
        _cliQueue(cliQueue) {}

protected:
  void run() override;

private:
  void handle(const LedTestRequest &request);
  void handle(const ButtonPressEvent &event);
  void handle(const SocialSetRequest &request);

  /// Map a button to its social key + colour.  Returns false for unmapped buttons.
  static bool buttonToSocial(hw::Button btn, storage::SocialKey &key, uint8_t &r, uint8_t &g, uint8_t &b);

  /// Convert a 0-255 value to a 1-18 pixel count.
  static uint8_t valueToPixelCount(uint8_t value);

  /// Check if all four social values are 255 (max).
  static bool allSocialMaxed();

  Queue<ControllerEvent> &_inQueue;
  Queue<LedCommand> &_ledQueue;
  Queue<CliResponse> &_cliQueue;

  hw::Button _lastButton = hw::Button::COUNT;  // tracks double-press
  bool _holdActive = false;                    // toggles on repeated same-button
};

}  // namespace core
