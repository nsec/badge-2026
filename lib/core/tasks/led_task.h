#pragma once

#include <memory>

#include <badge_config.h>

#include "animation/engine.h"
#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/led.h"

namespace core {

class LedTask : public Task {
public:
  explicit LedTask(Queue<LedCommand> &queue) : Task("led", badge::config::tasks::priority_led, 8192), _queue(queue) {}

protected:
  void run() override;

private:
  /// Sleep for `ms` but return early if the queue has a new command.
  /// Returns true (interrupted) if a new command was received into `out`.
  bool sleepOrInterrupt(uint32_t ms, LedCommand &out);

  /// Run progress flash animation. Returns true if interrupted (next cmd in `out`).
  bool runProgressFlash(const LedCommand &cmd, LedCommand &out);

  /// Run interruptible rainbow. Returns true if interrupted (next cmd in `out`).
  bool runRainbow(const LedCommand &cmd, LedCommand &out);

  /// Run a field-based animation from cmd.animation.
  /// Takes ownership of the AnimationDef pointer.
  /// Returns true if interrupted (next cmd in `out`).
  bool runAnimation(LedCommand &cmd, LedCommand &out);

  /// Drive `_currentAnimation` to completion through the animation engine.
  /// Returns true if interrupted (next cmd in `out`).
  bool driveAnimation(LedCommand &out);

  Queue<LedCommand> &_queue;
  animation::AnimationEngine _animEngine;
  std::unique_ptr<animation::AnimationDef> _currentAnimation;
};

namespace led {

void solidColor(uint8_t r, uint8_t g, uint8_t b);
void pixelWalk();
void rainbow();
void off();

}  // namespace led
}  // namespace core
