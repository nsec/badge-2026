#pragma once
/**
 * serial_mutex.h — Thread-safe serial output for ESP32-S3.
 *
 * Provides safeSerial() which returns a reference to the global
 * LineBufferedStream wrapping Serial.  The LBS has its own mutex
 * and 4KB buffer, so callers never need to lock manually.
 *
 * Usage from non-CLI tasks:
 *   #include "hardware/serial_mutex.h"
 *   core::hw::safeSerial().println("hello from NfcTask");
 *
 * Or use the LSerial macro:
 *   #define LSerial core::hw::safeSerial()
 *   LSerial.printf("...");
 */

#include "hardware/line_buffered_stream.h"

namespace core {
namespace hw {

/// Set the global safe-serial instance (called once from main.cpp setup).
void setSafeSerial(LineBufferedStream *lbs);

/// Get a reference to the global LineBufferedStream for thread-safe output.
LineBufferedStream &safeSerial();

}  // namespace hw
}  // namespace core
