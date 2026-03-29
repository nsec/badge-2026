#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/led.h"
#include "hardware/dock.h"

namespace core {

/// Task that monitors dock I2C events and drives badge LEDs accordingly.
class DockTask : public Task {
public:
  explicit DockTask(Queue<DockEvent> &dockQueue, Queue<LedCommand> &ledQueue)
      : Task("dock", badge::config::tasks::priority_dock), _dockQueue(dockQueue), _ledQueue(ledQueue) {}

protected:
  void run() override;

private:
  Queue<DockEvent> &_dockQueue;
  Queue<LedCommand> &_ledQueue;
};

}  // namespace core
