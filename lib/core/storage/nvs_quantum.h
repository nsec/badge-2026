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

/// Read a quantum string value (e.g. "flag1"). Returns true if found.
bool quantumReadStr(const char *key, char *buf, size_t bufSize);

/// Write a quantum string value.
void quantumWriteStr(const char *key, const char *value);

/// Read a quantum blob value. Returns actual bytes read, or 0 on error/not found.
size_t quantumReadBlob(const char *key, void *buf, size_t bufSize);

/// Write a quantum blob value. Returns true on success.
bool quantumWriteBlob(const char *key, const void *data, size_t len);

/// Erase a quantum NVS key. Returns true if erased.
bool quantumErase(const char *key);

}  // namespace storage
}  // namespace core
