#include "tasks/controller_task.h"

#include <Arduino.h>

namespace core {

Queue<ControllerEvent> *g_controllerQueue = nullptr;

void ControllerTask::run() {
  for (;;) {
    ControllerEvent event;
    if (!_inQueue.receive(event))
      continue;

    std::visit(
        [this](const auto &data) {
          handle(data);
        },
        event);
  }
}

void ControllerTask::handle(const LedTestRequest &req) {
  constexpr LedCommandType sequence[] = {
      LedCommandType::SolidRed,  LedCommandType::SolidGreen, LedCommandType::SolidBlue, LedCommandType::SolidWhite,
      LedCommandType::PixelWalk, LedCommandType::Rainbow,    LedCommandType::Off,
  };
  constexpr uint8_t count = sizeof(sequence) / sizeof(sequence[0]);
  const uint8_t total = req.testNum ? 1 : count;

  uint8_t step = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (req.testNum && *req.testNum != i + 1)
      continue;
    step++;
    if (req.progress) {
      req.progress(step, total, sequence[i]);
    }
    _ledQueue.send(LedCommand{sequence[i]});
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  _cliQueue.send(CliResponse{CliResponseType::LedTestComplete});
}

}  // namespace core
