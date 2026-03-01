#include "tasks/controller_task.h"

#include <Arduino.h>

namespace core {

Queue<ControllerEvent> *g_controllerQueue = nullptr;

void ControllerTask::run() {
  for (;;) {
    ControllerEvent event;
    if (!_inQueue.receive(event)) continue;

    switch (event.type) {
      case ControllerEventType::LedTestRequest:
        handleLedTest(event);
        break;
    }
  }
}

void ControllerTask::handleLedTest(const ControllerEvent &event) {
  constexpr LedCommandType sequence[] = {
      LedCommandType::SolidRed,   LedCommandType::SolidGreen,
      LedCommandType::SolidBlue,  LedCommandType::SolidWhite,
      LedCommandType::PixelWalk,  LedCommandType::Rainbow,
      LedCommandType::Off,
  };
  constexpr uint8_t count = sizeof(sequence) / sizeof(sequence[0]);

  for (uint8_t i = 0; i < count; i++) {
    if (event.ledTest.progress) {
      event.ledTest.progress(i + 1, count, sequence[i]);
    }
    _ledQueue.send(LedCommand{sequence[i]});
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  _cliQueue.send(CliResponse{CliResponseType::LedTestComplete});
}

}  // namespace core
