#include "tasks/dock_task.h"

#include <Arduino.h>
#include "hardware/serial_mutex.h"

#include "hardware/dock.h"
#include "hardware/rgb_led.h"
#include "storage/nvs_social.h"

#include <nvs_flash.h>
#include <nvs.h>

// Access the volatile flags from dock.cpp
extern volatile core::hw::DockLedColor g_pendingLedColor;
extern volatile bool g_ledColorPending;
extern volatile uint8_t g_pendingDockId;
extern volatile bool g_dockIdPending;
extern volatile uint8_t g_challengeSubOpcode;
extern volatile uint8_t g_challengeData[];
extern volatile uint8_t g_challengeDataLen;
extern volatile bool g_challengeDataPending;

// Challenge handler registry (defined in dock.cpp)
#define MAX_CHALLENGE_HANDLERS 8

struct ChallengeHandlerEntry {
  uint8_t subOpcode;
  core::hw::DockChallengeHandler handler;
};

extern ChallengeHandlerEntry g_challengeHandlers[];
extern uint8_t g_challengeHandlerCount;

namespace {

// Track which dock IDs we've seen (bitmask for IDs 1-255, stored in NVS)
// We use a simple NVS key "docks_seen" to persist an array of seen IDs.
// For simplicity with NVS, we track up to 32 docks in a uint32_t bitmask
// (dock IDs 1-32). This covers more than the 16 needed.
static constexpr const char *DOCK_NVS_NAMESPACE = "social";  // reuse existing NVS namespace

// In-memory set of seen dock IDs (loaded from NVS on first use)
uint32_t g_seenDockMask = 0;
bool g_seenDockLoaded = false;

void loadSeenDocks() {
  if (g_seenDockLoaded)
    return;
  // Read the sponsor value — we use it to derive how many docks were seen
  // But we need a separate NVS key for the bitmask. Use nvs directly.
  g_seenDockLoaded = true;

  // Try reading from NVS
  nvs_handle_t handle;
  if (nvs_open("docks", NVS_READWRITE, &handle) == ESP_OK) {
    nvs_get_u32(handle, "seen", &g_seenDockMask);
    nvs_close(handle);
  }
}

void saveSeenDocks() {
  nvs_handle_t handle;
  if (nvs_open("docks", NVS_READWRITE, &handle) == ESP_OK) {
    nvs_set_u32(handle, "seen", g_seenDockMask);
    nvs_commit(handle);
    nvs_close(handle);
  }
}

uint8_t countSeenDocks() {
  uint32_t v = g_seenDockMask;
  uint8_t count = 0;
  while (v) {
    count += v & 1;
    v >>= 1;
  }
  return count;
}

}  // namespace

namespace core {

void DockTask::run() {
  for (;;) {
    // Handle dock ID
    if (g_dockIdPending) {
      g_dockIdPending = false;
      uint8_t dockId = g_pendingDockId;

      loadSeenDocks();

      // Dock IDs 1-32 tracked via bitmask
      if (dockId >= 1 && dockId <= 32) {
        uint32_t bit = 1u << (dockId - 1);
        if (!(g_seenDockMask & bit)) {
          // New dock!
          g_seenDockMask |= bit;
          saveSeenDocks();

          uint8_t seen = countSeenDocks();
          // 16 docks = 255 exactly.  Use rounding: (seen * 255 + 15) / 16
          uint16_t sponsorVal = ((uint16_t)seen * 255 + (hw::MAX_SPONSOR_DOCKS - 1)) / hw::MAX_SPONSOR_DOCKS;
          if (sponsorVal > 255)
            sponsorVal = 255;
          storage::socialWrite(storage::SocialKey::Sponsor, (uint8_t)sponsorVal);

          core::hw::safeSerial().printf("Dock: NEW dock #%d seen (%d total), sponsor=%d\r\n", dockId, seen, sponsorVal);
        } else {
          core::hw::safeSerial().printf("Dock: dock #%d already seen\r\n", dockId);
        }
      }
    }

    // Handle challenge data from dock
    if (g_challengeDataPending) {
      g_challengeDataPending = false;
      uint8_t subOp = g_challengeSubOpcode;
      uint8_t dataCopy[32];
      uint8_t dataLen = g_challengeDataLen;
      memcpy(dataCopy, const_cast<uint8_t *>(const_cast<volatile uint8_t *>(g_challengeData)), dataLen);

      bool handled = false;
      for (uint8_t i = 0; i < g_challengeHandlerCount; i++) {
        if (g_challengeHandlers[i].subOpcode == subOp) {
          g_challengeHandlers[i].handler(subOp, dataCopy, dataLen);
          handled = true;
          break;
        }
      }
      if (!handled) {
        core::hw::safeSerial().printf("Dock: unhandled challenge sub-opcode 0x%02X\r\n", subOp);
      }
    }

    // Handle LED color
    if (g_ledColorPending) {
      g_ledColorPending = false;
      hw::DockLedColor color = g_pendingLedColor;

      LedCommand cmd{};
      cmd.type = LedCommandType::ProgressFlash;
      cmd.pixelCount = 18;

      switch (color) {
        case hw::DockLedColor::Red:
          cmd.r = 255;
          cmd.g = 0;
          cmd.b = 0;
          break;
        case hw::DockLedColor::Green:
          cmd.r = 0;
          cmd.g = 255;
          cmd.b = 0;
          break;
        case hw::DockLedColor::Blue:
          cmd.r = 0;
          cmd.g = 0;
          cmd.b = 255;
          break;
        case hw::DockLedColor::Off:
        default:
          cmd.type = LedCommandType::Off;
          break;
      }

      _ledQueue.send(cmd, Milliseconds(0));
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace core
