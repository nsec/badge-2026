#pragma once

#include <Arduino.h>

namespace core {
namespace hw {

/**
 * Get the unique hardware ID for this badge.
 *
 * Returns the ESP32-S3 base MAC address as a 12-character
 * uppercase hex string (e.g. "4827E2E91584").
 * This value is unique per chip and burned into eFuse at the factory.
 */
String getHardwareId();

/**
 * Print the hardware ID to a stream.
 */
void printHardwareId(Stream &io);

}  // namespace hw
}  // namespace core
