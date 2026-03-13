#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/controller.h"
#include "tasks/led.h"
#include "tasks/cli_queue.h"
#include "tasks/nfc.h"

namespace core {

class ControllerTask : public Task {
public:
  ControllerTask(Queue<ControllerEvent> &inQueue, Queue<LedCommand> &ledQueue, Queue<CliResponse> &cliQueue,
                 Queue<NfcCommand> &nfcQueue)
      : Task("controller", badge::config::tasks::priority_controller), _inQueue(inQueue), _ledQueue(ledQueue),
        _cliQueue(cliQueue), _nfcQueue(nfcQueue) {}

protected:
  void run() override;

private:
  void handle(const LedTestRequest &request);
  void handle(const ButtonPressEvent &event);
  void handle(const SocialSetRequest &request);

  /// Get colour for a social category.
  static void socialColor(storage::SocialKey key, uint8_t &r, uint8_t &g, uint8_t &b);

  /// Convert a 0-255 value to a 1-18 pixel count.
  static uint8_t valueToPixelCount(uint8_t value);

  /// Check if all four social values are 255 (max).
  static bool allSocialMaxed();

  /// Show the current social category on the LEDs.
  void showCurrentSocial(bool hold);

  Queue<ControllerEvent> &_inQueue;
  Queue<LedCommand> &_ledQueue;
  Queue<CliResponse> &_cliQueue;
  Queue<NfcCommand> &_nfcQueue;

  // Social display state
  uint8_t _socialIndex = 0;             // current category index (0-3)
  bool _socialActive = false;           // UP was pressed at least once
  hw::Button _lastButton = hw::Button::COUNT;
  bool _holdActive = false;

  // Brightness state (1-10, default 5)
  uint8_t _brightnessLevel = 5;
};

}  // namespace core
