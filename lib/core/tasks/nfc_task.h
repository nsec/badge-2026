#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/display.h"
#include "tasks/nfc.h"
#include "tasks/led.h"

namespace core {

class NfcTask : public Task {
public:
  NfcTask(Queue<NfcCommand> &nfcQueue, Queue<LedCommand> &ledQueue, Queue<DisplayCommand> &displayQueue)
      : Task("nfc", badge::config::tasks::priority_nfc, 8192), _nfcQueue(nfcQueue), _ledQueue(ledQueue),
        _displayQueue(displayQueue) {}

protected:
  void run() override;

private:
  void runReader();
  void runEmulator();
  void runPair();

  /// Check for a new command without blocking.  Returns true if one was received.
  bool checkCommand(NfcCommand &out);

  Queue<NfcCommand> &_nfcQueue;
  Queue<LedCommand> &_ledQueue;
  Queue<DisplayCommand> &_displayQueue;
};

}  // namespace core
