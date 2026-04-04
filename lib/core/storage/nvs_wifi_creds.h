#pragma once

#include <badge_config.h>

namespace core {
namespace storage {

struct WifiCredentials {
  char ssid[badge::config::wifi::ssid_max_len + 1] = {};
  char passphrase[badge::config::wifi::passphrase_len + 1] = {};
};

/**
 * Get WiFi AP credentials, generating and persisting them on first call.
 *
 * On the first invocation (no credentials in NVS): builds the SSID from the
 * last 3 MAC bytes, generates a random WPA2 passphrase via esp_random(),
 * stores both in NVS, and returns them.
 *
 * On subsequent calls: returns the cached credentials (no NVS I/O).
 */
WifiCredentials wifiCredsGet();

}  // namespace storage
}  // namespace core
