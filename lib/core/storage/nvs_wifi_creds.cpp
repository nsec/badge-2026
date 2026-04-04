#include "storage/nvs_wifi_creds.h"

#include <Arduino.h>
#include <nvs.h>
#include <esp_random.h>
#include <bootloader_random.h>
#include <cstring>

#include "hardware/hwid.h"

namespace {

nvs_handle_t wifi_handle = 0;
bool initialized = false;
core::storage::WifiCredentials cached;

static constexpr const char *NS = "wifi";
static constexpr const char *SSID_KEY = "ssid";
static constexpr const char *PASS_KEY = "pass";

static constexpr const char ALPHANUM[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static constexpr size_t ALPHANUM_LEN = sizeof(ALPHANUM) - 1;  // 62

void generatePassphrase(char *out, size_t len) {
  bootloader_random_enable();
  for (size_t i = 0; i < len; i++) {
    uint32_t r = esp_random();
    out[i] = ALPHANUM[r % ALPHANUM_LEN];
  }
  bootloader_random_disable();
  out[len] = '\0';
}

void generateSsid(char *out, size_t maxLen) {
  uint8_t mac[core::hw::MAC_LEN];
  core::hw::getHwidMac(mac);
  snprintf(out, maxLen, "NSEC-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

bool loadFromNvs() {
  size_t ssidLen = sizeof(cached.ssid);
  if (nvs_get_str(wifi_handle, SSID_KEY, cached.ssid, &ssidLen) != ESP_OK)
    return false;

  size_t passLen = sizeof(cached.passphrase);
  if (nvs_get_str(wifi_handle, PASS_KEY, cached.passphrase, &passLen) != ESP_OK)
    return false;

  return cached.ssid[0] != '\0' && cached.passphrase[0] != '\0';
}

void generateAndStore() {
  generateSsid(cached.ssid, sizeof(cached.ssid));
  generatePassphrase(cached.passphrase, badge::config::wifi::passphrase_len);

  nvs_set_str(wifi_handle, SSID_KEY, cached.ssid);
  nvs_set_str(wifi_handle, PASS_KEY, cached.passphrase);
  nvs_commit(wifi_handle);

  Serial.printf("NVS: generated WiFi creds SSID=\"%s\" pass=\"%s\"\r\n", cached.ssid, cached.passphrase);
}

}  // namespace

namespace core {
namespace storage {

WifiCredentials wifiCredsGet() {
  if (initialized)
    return cached;

  esp_err_t err = nvs_open(NS, NVS_READWRITE, &wifi_handle);
  if (err != ESP_OK) {
    Serial.printf("NVS: open '%s' namespace failed (%d)\r\n", NS, err);
    return cached;
  }

  if (!loadFromNvs()) {
    generateAndStore();
  } else {
    Serial.printf("NVS: loaded WiFi creds SSID=\"%s\"\r\n", cached.ssid);
  }

  initialized = true;
  return cached;
}

}  // namespace storage
}  // namespace core
