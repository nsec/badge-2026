#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/led.h"

namespace core {

/// Task that monitors dock I2C commands and drives badge LEDs accordingly.
class DockTask : public Task {
public:
  explicit DockTask(Queue<LedCommand> &ledQueue)
      : Task("dock", badge::config::tasks::priority_dock), _ledQueue(ledQueue) {}

protected:
  void run() override;

private:
  Queue<LedCommand> &_ledQueue;
};

}  // namespace core
