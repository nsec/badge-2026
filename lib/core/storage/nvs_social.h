#pragma once

#include <cstdint>

namespace core {
namespace storage {

/// The four social progress categories.
enum class SocialKey : uint8_t {
  Social,      // citizens/players — Down button
  Sponsor,     // vendors — Up button
  Light,       // light collection — Left button
  Attraction,  // attractions — Right button
  COUNT
};

static constexpr uint8_t SOCIAL_KEY_COUNT = static_cast<uint8_t>(SocialKey::COUNT);

/**
 * Initialize NVS and open the "social" namespace.
 * Must be called once at startup (from the Arduino thread, before tasks).
 */
void socialNvsInit();

/**
 * Read a social progress value from NVS.
 * Internally decrypts a JSON blob stored in NVS by XOR'ing with
 * the full 6-byte hardware MAC (cycling).  Returns 0 if never written.
 */
uint8_t socialRead(SocialKey key);

/**
 * Write a social progress value to NVS.
 * The entire JSON blob containing all four values is re-encrypted
 * with the full 6-byte hardware MAC before storage, making each
 * badge's NVS data unique and opaque.
 * @param key   Which category to write.
 * @param value Raw progress value 0-255.
 */
void socialWrite(SocialKey key, uint8_t value);

/**
 * Get a human-readable name for a social key.
 */
const char *socialKeyName(SocialKey key);

}  // namespace storage
}  // namespace core
