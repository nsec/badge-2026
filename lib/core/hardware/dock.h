#pragma once

#include <cstdint>

namespace core {
namespace hw {

/// I2C slave address the badge listens on for dock communication.
static constexpr uint8_t DOCK_I2C_ADDR = 0x68;

/// Dock I2C command codes (sent by dock, handled by badge).
enum class DockCmd : uint8_t {
  RequestHwid = 0x01,  // Dock requests the badge's 12-byte hex hardware ID
  SetLedColor = 0x02,  // Dock tells the badge to set LED color (1 byte payload)
  SendDockId = 0x03,   // Dock sends its ID (1 byte: 1-255)
};

/// LED color codes sent by dock.
enum class DockLedColor : uint8_t {
  Off = 0x00,
  Red = 0x01,
  Green = 0x02,
  Blue = 0x03,
};

/// Initialize the I2C slave for dock communication.
/// Must be called after Wire/I2C is available.
void dockInit();

/// Maximum number of unique docks that fill the sponsor bar.
static constexpr uint8_t MAX_SPONSOR_DOCKS = 16;

}  // namespace hw
}  // namespace core
