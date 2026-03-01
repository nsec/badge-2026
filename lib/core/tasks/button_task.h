#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/controller.h"

namespace core {

/**
 * Polls all physical buttons and sends ButtonPressEvent to the controller
 * queue on each new press edge (with debounce).
 */
class ButtonTask : public Task {
public:
  explicit ButtonTask(Queue<ControllerEvent> &controllerQueue)
      : Task("buttons", badge::config::tasks::priority_button), _controllerQueue(controllerQueue) {}

protected:
  void run() override;

private:
  Queue<ControllerEvent> &_controllerQueue;
};

}  // namespace core
