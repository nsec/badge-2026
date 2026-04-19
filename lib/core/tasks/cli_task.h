#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "tasks/cli.h"

namespace core {

class CliTask : public Task {
public:
  CliTask() : Task("cli", badge::config::tasks::priority_cli, 8192) {}

protected:
  void run() override;
};

}  // namespace core
