#include "tasks/dock_task.h"

#include <Arduino.h>
#include "hardware/serial_mutex.h"

#include "hardware/dock.h"
#include "hardware/rgb_led.h"
#include "storage/nvs_social.h"

#include <nvs_flash.h>
#include <nvs.h>

// Challenge handler registry (struct + array defined in dock.h / dock.cpp)
extern core::hw::ChallengeHandlerEntry g_challengeHandlers[];
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
    DockEvent evt;
    if (!_dockQueue.receive(evt, Milliseconds(100)))
      continue;

    switch (evt.type) {
      case DockEventType::DockId: {
        uint8_t dockId = evt.dockId;
        loadSeenDocks();

        if (dockId >= 1 && dockId <= 32) {
          uint32_t bit = 1u << (dockId - 1);
          if (!(g_seenDockMask & bit)) {
            g_seenDockMask |= bit;
            saveSeenDocks();

            uint8_t seen = countSeenDocks();
            uint16_t sponsorVal = ((uint16_t)seen * 255 + (hw::MAX_SPONSOR_DOCKS - 1)) / hw::MAX_SPONSOR_DOCKS;
            if (sponsorVal > 255)
              sponsorVal = 255;
            storage::socialWrite(storage::SocialKey::Sponsor, (uint8_t)sponsorVal);

            core::hw::safeSerial().printf("Dock: NEW dock #%d seen (%d total), sponsor=%d\r\n", dockId, seen,
                                          sponsorVal);
          } else {
            core::hw::safeSerial().printf("Dock: dock #%d already seen\r\n", dockId);
          }
        }
        break;
      }

      case DockEventType::ChallengeData: {
        bool handled = false;
        for (uint8_t i = 0; i < g_challengeHandlerCount; i++) {
          if (g_challengeHandlers[i].subOpcode == evt.challenge.subOpcode) {
            g_challengeHandlers[i].handler(evt.challenge.subOpcode, evt.challenge.data, evt.challenge.dataLen);
            handled = true;
            break;
          }
        }
        if (!handled) {
          core::hw::safeSerial().printf("Dock: unhandled challenge sub-opcode 0x%02X\r\n", evt.challenge.subOpcode);
        }
        break;
      }

      case DockEventType::LedColor: {
        LedCommand cmd{};
        cmd.type = LedCommandType::ProgressFlash;
        cmd.pixelCount = 18;

        switch (evt.ledColor) {
          case hw::DockLedColor::Red:
            cmd.r = 255;
            break;
          case hw::DockLedColor::Green:
            cmd.g = 255;
            break;
          case hw::DockLedColor::Blue:
            cmd.b = 255;
            break;
          case hw::DockLedColor::Off:
          default:
            cmd.type = LedCommandType::Off;
            break;
        }
        _ledQueue.send(cmd, Milliseconds(0));
        break;
      }
    }
  }
}

}  // namespace core
