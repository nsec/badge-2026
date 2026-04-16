#pragma once

#include <cstdint>

namespace core {
namespace hw {

/// I2C slave address the badge listens on for dock communication.
static constexpr uint8_t DOCK_I2C_ADDR = 0x68;

/// Dock I2C command codes (sent by dock, handled by badge).
enum class DockCmd : uint8_t {
  RequestHwid = 0x01,    // Dock requests the badge's 12-byte hex hardware ID
  SetLedColor = 0x02,    // Dock tells the badge to set LED color (1 byte payload)
  SendDockId = 0x03,     // Dock sends its ID (1 byte: 1-255)
  ChallengeData = 0x10,  // Challenge dock sends opcode + payload (routed to registered handler)
};

/// LED color codes sent by dock.
enum class DockLedColor : uint8_t {
  Off = 0x00,
  Red = 0x01,
  Green = 0x02,
  Blue = 0x03,
};

/// Callback type for challenge dock handlers.
/// Called from the dock task context (not ISR) with the sub-opcode and payload.
typedef void (*DockChallengeHandler)(uint8_t subOpcode, const uint8_t *data, uint8_t dataLen);

// ---------------------------------------------------------------------------
// Challenge handler registry (shared between dock.cpp and dock_task.cpp)
// ---------------------------------------------------------------------------

static constexpr uint8_t MAX_CHALLENGE_HANDLERS = 8;
static constexpr uint8_t CHALLENGE_DATA_MAX = 32;

struct ChallengeHandlerEntry {
  uint8_t subOpcode;
  DockChallengeHandler handler;
};

/// Initialize the I2C slave for dock communication.
void dockInit();

/// Re-attach the I2C slave after a temporary master transaction (e.g. light sensor).
/// Cheaper than dockInit(): skips HWID setup, just re-inits Wire + callbacks.
void dockReinitSlave();

/// Register a handler for ChallengeData commands with a specific sub-opcode.
/// Multiple handlers can be registered for different sub-opcodes.
void dockRegisterChallengeHandler(uint8_t subOpcode, DockChallengeHandler handler);

/// Set a custom response buffer for the next I2C master read.
/// If set, the next onRequest sends this instead of the HWID. Cleared after one read.
void dockSetResponseBuffer(const uint8_t *data, uint8_t len);

/// Set the dock event queue (called from main.cpp before tasks start).
void dockSetEventQueue(void *queueHandle);

/// Returns true if the badge has received dock I2C activity recently (within ~3s).
/// Used by the light sensor to avoid bus contention while docked.
bool dockIsActive();

/// Maximum number of unique docks that fill the sponsor bar.
static constexpr uint8_t MAX_SPONSOR_DOCKS = 16;

}  // namespace hw

// ---------------------------------------------------------------------------
// Dock event — sent from I2C ISR to DockTask via FreeRTOS queue.
// Lives in core:: namespace alongside other task event types
// (ControllerEvent, LedCommand, NfcCommand, etc.)
// ---------------------------------------------------------------------------

enum class DockEventType : uint8_t {
  LedColor,
  DockId,
  ChallengeData,
};

struct DockEvent {
  DockEventType type;

  union {
    hw::DockLedColor ledColor;
    uint8_t dockId;

    struct {
      uint8_t subOpcode;
      uint8_t data[hw::CHALLENGE_DATA_MAX];
      uint8_t dataLen;
    } challenge;
  };
};

}  // namespace core
