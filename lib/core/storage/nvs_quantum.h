#pragma once

#include <cstdint>
#include <cstddef>

namespace core {
namespace storage {

/// Initialize the quantum NVS namespace.
void quantumNvsInit();

/// Read a quantum uint8 value (e.g. "quantum1").
uint8_t quantumRead(const char *key);

/// Write a quantum uint8 value.
void quantumWrite(const char *key, uint8_t value);

/// Read a quantum string value (e.g. "flag1"). Returns empty string if not set.
/// Buffer must be at least `bufSize` bytes.  Returns true if found.
bool quantumReadStr(const char *key, char *buf, size_t bufSize);

/// Write a quantum string value.
void quantumWriteStr(const char *key, const char *value);

}  // namespace storage
}  // namespace core
