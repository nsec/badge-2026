#pragma once

#include <array>
#include <cstdint>

#include "badge_config.h"

namespace core {
namespace storage {

using MacAddress = std::array<uint8_t, 6>;

struct ContactProfile {
  MacAddress mac = {};
  char name[badge::config::profile::name_max_len + 1] = {0};
  char pronouns[badge::config::profile::pronouns_max_len + 1] = {0};
  char affiliation[badge::config::profile::affiliation_max_len + 1] = {0};
  char contact[badge::config::profile::contact_max_len + 1] = {0};
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

/**
 * Store a contact profile in NVS, keyed by MAC address.
 * Overwrites any existing entry for the same MAC.
 */
void contactWrite(const ContactProfile &profile);

/**
 * Get the number of stored contacts.
 */
uint16_t contactCount();

/**
 * Get a contact by index (0-based).
 * Returns false if index is out of range.
 */
bool contactGet(uint16_t index, ContactProfile &out);

/**
 * Delete a single contact by MAC address.
 * Returns true if found and deleted.
 */
bool contactDelete(const MacAddress &mac);

/**
 * Erase all stored contacts from NVS.
 */
void contactReset();

}  // namespace storage
}  // namespace core
