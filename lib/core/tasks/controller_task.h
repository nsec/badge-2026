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
  ControllerTask(Queue<ControllerEvent> &inQueue, Queue<LedCommand> &ledQueue,
                 Queue<CliResponse> &cliQueue)
      : Task("controller", badge::config::tasks::priority_controller),
        _inQueue(inQueue),
        _ledQueue(ledQueue),
        _cliQueue(cliQueue) {}

protected:
  void run() override;

private:
  void handle(const LedTestRequest &request);

  Queue<ControllerEvent> &_inQueue;
  Queue<LedCommand> &_ledQueue;
  Queue<CliResponse> &_cliQueue;
};

}  // namespace core
