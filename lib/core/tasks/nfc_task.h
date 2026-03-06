#pragma once

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/nfc.h"
#include "tasks/led.h"

namespace core {

class NfcTask : public Task {
public:
  NfcTask(Queue<NfcCommand> &nfcQueue, Queue<LedCommand> &ledQueue)
      : Task("nfc", badge::config::tasks::priority_nfc, 8192), _nfcQueue(nfcQueue), _ledQueue(ledQueue) {}

protected:
  void run() override;

private:
  void runReader();
  void runEmulator();

  /// Check for a new command without blocking.  Returns true if one was received.
  bool checkCommand(NfcCommand &out);

  Queue<NfcCommand> &_nfcQueue;
  Queue<LedCommand> &_ledQueue;
};

}  // namespace core
