#pragma once

#include <Arduino.h>
#include <cstdint>

namespace core {
namespace hw {

/// MAC address length.
static constexpr uint8_t MAC_LEN = 6;

/**
 * Copy the factory MAC address into `out` (6 bytes).
 */
void getHwidMac(uint8_t out[MAC_LEN]);

/**
 * Derive a single obfuscation byte from the full MAC address.
 * Uses multiplicative mixing of all 6 bytes so the mapping is
 * harder to reverse than a simple XOR fold.
 */
uint8_t getHwidObfuscationByte();

/**
 * Print the hardware ID (full MAC hex string) to a stream.
 */
void printHardwareId(Stream &io);

}  // namespace hw
}  // namespace core
