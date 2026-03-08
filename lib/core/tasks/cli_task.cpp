#include "tasks/cli_task.h"

#include <Arduino.h>

#include "tasks/cli.h"
#include "tasks/cli_queue.h"

namespace core {

Queue<CliResponse> *g_cliQueue = nullptr;

void CliTask::run() {
  for (;;) {
    cli::poll();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace core
