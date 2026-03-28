#pragma once

#include <badge_config.h>

#include <freertos/timers.h>

#include "rtos/queue.hpp"
#include "rtos/task.hpp"
#include "tasks/display.h"

namespace core {

class DisplayTask : public Task {
public:
  explicit DisplayTask(Queue<DisplayCommand> &queue);

protected:
  void run() override;

private:
  Queue<DisplayCommand> &_queue;
  TimerHandle_t _timer;

  static void onTimeout(TimerHandle_t timer);

  void showLogo();
  void showModeChange(const DisplayCommand &cmd);
  void showNfcScan(const DisplayCommand &cmd);
  void showPairResult(const DisplayCommand &cmd);
  void showSocialProgress(const DisplayCommand &cmd);

  void renderText(const char *line1, const char *line2 = nullptr, const char *line3 = nullptr);
};

}  // namespace core
