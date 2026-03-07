#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "network/wifi_portal.h"
#include "tasks/controller.h"

namespace core {

class PortalTask : public Task {
public:
  PortalTask(Queue<PortalCommand> &cmdQueue, Queue<ControllerEvent> &controllerQueue)
      : Task("portal", badge::config::tasks::priority_portal, 8192),
        _cmdQueue(cmdQueue),
        _controllerQueue(controllerQueue) {}

protected:
  void run() override;

private:
  void startPortal();
  void stopPortal();

  Queue<PortalCommand> &_cmdQueue;
  Queue<ControllerEvent> &_controllerQueue;
  bool _running = false;
};

}  // namespace core
