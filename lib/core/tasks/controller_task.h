#pragma once

#include <optional>
#include <string>
#include <vector>

#include <badge_config.h>

#include "rtos/task.hpp"
#include "rtos/queue.hpp"
#include "tasks/controller.h"
#include "tasks/display.h"
#include "tasks/led.h"
#include "tasks/cli_queue.h"
#include "tasks/nfc.h"
#include "network/wifi_portal.h"

namespace core {

class ControllerTask : public Task {
public:
  ControllerTask(Queue<ControllerEvent> &inQueue, Queue<LedCommand> &ledQueue, Queue<CliResponse> &cliQueue,
                 Queue<NfcCommand> &nfcQueue, Queue<DisplayCommand> &displayQueue, Queue<PortalCommand> &portalQueue)
      : Task("controller", badge::config::tasks::priority_controller), _inQueue(inQueue), _ledQueue(ledQueue),
        _cliQueue(cliQueue), _nfcQueue(nfcQueue), _displayQueue(displayQueue), _portalQueue(portalQueue) {}

protected:
  void run() override;

private:
  enum class State : uint8_t {
    Idle,
    Social,
    NfcReader,
    NfcEmulator,
    NfcPair,
    Portal,
  };

  void handle(const LedTestRequest &request);
  void handle(const ButtonPressEvent &event);
  void handle(const SocialSetRequest &request);
  void handle(const PortalToggleRequest &request);
  void handle(const ConfigChangedEvent &event);
  void handle(const NfcScanResultEvent &event);
  void handle(const NfcPairResultEvent &event);

  // State transitions. Each routes through cleanupCurrent() so the previous
  // state's external resources (NFC, portal task) are released before the
  // new state's hardware is set up.
  //
  // enterIdle's `deferMs` keeps the idle animation queued for later — used
  // after a result-flash so the flash plays out before the idle anim
  // overwrites it.
  void enterIdle(uint32_t deferMs = 0);
  void enterSocial();
  void enterNfcMode(NfcMode mode);
  void enterPortal();

  // Stop whatever the current state was doing (NFC, portal task, etc.).
  void cleanupCurrent();

  // A-in-Idle: advance to the next idle animation.
  void cycleIdleAnimation();

  // Populate _idleAnimations from the filesystem (reserved names — boot,
  // wifi_portal — are excluded). Called once at task startup.
  void enumerateIdleAnimations();

  // Send the current idle animation (the boot animation if _idleIndex < 0,
  // otherwise kIdleAnimations[_idleIndex]).
  void sendCurrentIdleAnimation();

  // Load and queue an animation by name. No-op on parse failure.
  void sendNamedAnimation(const char *name);

  // Show the current social category. Always sent with hold so the breathe
  // continues until a timeout or another transition takes us out of Social.
  void showCurrentSocial();

  // Helpers (unchanged from before).
  static void socialColor(storage::SocialKey key, uint8_t &r, uint8_t &g, uint8_t &b);
  static uint8_t valueToPixelCount(uint8_t value);
  static bool allSocialMaxed();

  Queue<ControllerEvent> &_inQueue;
  Queue<LedCommand> &_ledQueue;
  Queue<CliResponse> &_cliQueue;
  Queue<NfcCommand> &_nfcQueue;
  Queue<DisplayCommand> &_displayQueue;
  Queue<PortalCommand> &_portalQueue;

  State _state = State::Idle;

  // millis() deadline for the next pending timer (0 = none). When _state is
  // Idle, this is a deferred idle-animation send (post-result-flash). When
  // _state is anything else, it is a timed transition back to Idle.
  uint32_t _timeoutAt = 0;

  // Idle animation cycle, discovered from the filesystem at task startup
  // (boot and wifi_portal are excluded — they are reserved).
  std::vector<std::string> _idleAnimations;

  // Idle animation index. std::nullopt means the boot animation is still
  // playing — boot is shown once at startup and dropped on the first
  // transition out of idle. Afterwards the index is in [0, size()).
  std::optional<size_t> _idleIndex;

  // Social sub-state.
  uint8_t _socialIndex = 0;
  bool _holdActive = false;
  hw::Button _lastButton = hw::Button::COUNT;

  // Brightness (1-10, default 5).
  uint8_t _brightnessLevel = 5;
};

}  // namespace core
