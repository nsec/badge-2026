#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/led.h"

namespace core {

class LedTask : public Task {
public:
  explicit LedTask(Queue<LedCommand> &queue)
      : Task("led", badge::config::tasks::priority_led), _queue(queue) {}

protected:
  void run() override;

private:
  Queue<LedCommand> &_queue;
};

namespace led {

void solidColor(uint8_t r, uint8_t g, uint8_t b);
void pixelWalk();
void rainbow();
void off();

}  // namespace led
}  // namespace core
