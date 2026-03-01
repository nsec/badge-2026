#include "storage/nvs_social.h"

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "hardware/hwid.h"

namespace {

nvs_handle_t g_nvsHandle = 0;
bool g_initialized = false;

/// Derive a single-byte key from the hardware ID, used to XOR stored values.
/// Folds all 6 MAC bytes into one via XOR so each badge gets a unique mask.
uint8_t hwidXorByte() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  uint8_t fold = 0;
  for (int i = 0; i < 6; i++) {
    fold ^= mac[i];
  }
  return fold;
}

const char *keyStr(core::storage::SocialKey key) {
  switch (key) {
    case core::storage::SocialKey::Social:
      return "social";
    case core::storage::SocialKey::Sponsor:
      return "sponsor";
    case core::storage::SocialKey::Light:
      return "light";
    case core::storage::SocialKey::Attraction:
      return "attraction";
    default:
      return "unknown";
  }
}

}  // namespace

namespace core {
namespace storage {

void socialNvsInit() {
  if (g_initialized)
    return;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    Serial.println("NVS: erasing and re-initializing...");
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.printf("NVS: flash init failed (%d)\n", err);
    return;
  }

  err = nvs_open("social", NVS_READWRITE, &g_nvsHandle);
  if (err != ESP_OK) {
    Serial.printf("NVS: open 'social' namespace failed (%d)\n", err);
    return;
  }

  g_initialized = true;
  Serial.println("NVS: 'social' namespace ready");
}

uint8_t socialRead(SocialKey key) {
  if (!g_initialized)
    return 0;

  uint8_t stored = 0;
  esp_err_t err = nvs_get_u8(g_nvsHandle, keyStr(key), &stored);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return 0;  // never written
  }
  if (err != ESP_OK) {
    Serial.printf("NVS: read '%s' failed (%d)\n", keyStr(key), err);
    return 0;
  }

  // XOR with hardware-id byte to recover the raw value
  return stored ^ hwidXorByte();
}

void socialWrite(SocialKey key, uint8_t value) {
  if (!g_initialized)
    return;

  // XOR with hardware-id byte before storing
  uint8_t blob = value ^ hwidXorByte();

  esp_err_t err = nvs_set_u8(g_nvsHandle, keyStr(key), blob);
  if (err != ESP_OK) {
    Serial.printf("NVS: write '%s' failed (%d)\n", keyStr(key), err);
    return;
  }

  err = nvs_commit(g_nvsHandle);
  if (err != ESP_OK) {
    Serial.printf("NVS: commit failed (%d)\n", err);
  }
}

const char *socialKeyName(SocialKey key) {
  return keyStr(key);
}

}  // namespace storage
}  // namespace core
