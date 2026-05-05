#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "rtos/queue.hpp"
#include "hardware/buttons.h"
#include "storage/nvs_social.h"
#include "network/wifi_portal.h"

namespace core {

// Forward-declare LedCommandType so the progress callback can reference it.
enum class LedCommandType : uint8_t;

using ProgressFn = void (*)(uint8_t step, uint8_t total, LedCommandType animation);

struct LedTestRequest {
  ProgressFn progress = nullptr;
  std::optional<uint8_t> testNum;  // nullopt = run all, 1-7 = single
};

/// A physical button was pressed (sent by the button-polling task).
struct ButtonPressEvent {
  hw::Button button;
};

/// CLI request to set a social NVS value (sent by the CLI nvstest command).
struct SocialSetRequest {
  storage::SocialKey key;
  uint8_t value;  // 0-255
};

/// Request the controller to start or stop the WiFi config portal.
struct PortalToggleRequest {};

/// NFC reader successfully scanned a tag — controller flashes the LEDs green.
struct NfcScanResultEvent {};

/// NFC-DEP pair sequence completed — controller flashes the LEDs accordingly.
struct NfcPairResultEvent {
  uint8_t outcome;  // 0 = new partner, 1 = duplicate, 2 = HMAC failed
};

using ControllerEvent = std::variant<LedTestRequest, ButtonPressEvent, SocialSetRequest, PortalToggleRequest,
                                     ConfigChangedEvent, NfcScanResultEvent, NfcPairResultEvent>;

extern Queue<ControllerEvent> *g_controllerQueue;

}  // namespace core
