#include "storage/nvs_config.h"

#include <Arduino.h>
#include <nvs.h>
#include <cstring>

#include "hardware/hwid.h"

namespace {

nvs_handle_t config_storage_handle = 0;
bool is_nvs_initialized = false;

static constexpr const char *NS = "config";

// Sentinel stored when the user has explicitly saved a color.
// Without this, we can't distinguish "never set" from "set to black".
static constexpr const char *COLOR_SET_KEY = "color_set";

}  // namespace

namespace core {
namespace storage {

void configNvsInit() {
  if (is_nvs_initialized) {
    return;
  }

  esp_err_t err = nvs_open(NS, NVS_READWRITE, &config_storage_handle);
  if (err != ESP_OK) {
    Serial.printf("NVS: open '%s' namespace failed (%d)\r\n", NS, err);
    return;
  }

  is_nvs_initialized = true;
  Serial.println("NVS: 'config' namespace ready");
}

UserConfig configRead() {
  UserConfig cfg;

  // Populate the profile's MAC with this badge's own address.
  hw::getHwidMac(cfg.profile.mac.data());

  if (!is_nvs_initialized) {
    return cfg;
  }

  size_t len = sizeof(cfg.profile.name);
  if (nvs_get_str(config_storage_handle, "name", cfg.profile.name, &len) != ESP_OK) {
    cfg.profile.name[0] = '\0';
  }

  len = sizeof(cfg.profile.pronouns);
  if (nvs_get_str(config_storage_handle, "pronouns", cfg.profile.pronouns, &len) != ESP_OK) {
    cfg.profile.pronouns[0] = '\0';
  }

  len = sizeof(cfg.profile.affiliation);
  if (nvs_get_str(config_storage_handle, "affil", cfg.profile.affiliation, &len) != ESP_OK) {
    cfg.profile.affiliation[0] = '\0';
  }

  len = sizeof(cfg.profile.contact);
  if (nvs_get_str(config_storage_handle, "contact", cfg.profile.contact, &len) != ESP_OK) {
    cfg.profile.contact[0] = '\0';
  }

  // Color: use defaults if never explicitly set.
  uint8_t colorSet = 0;
  nvs_get_u8(config_storage_handle, COLOR_SET_KEY, &colorSet);
  if (colorSet) {
    uint8_t val;
    if (nvs_get_u8(config_storage_handle, "r", &val) == ESP_OK) {
      cfg.profile.r = val;
    }
    if (nvs_get_u8(config_storage_handle, "g", &val) == ESP_OK) {
      cfg.profile.g = val;
    }
    if (nvs_get_u8(config_storage_handle, "b", &val) == ESP_OK) {
      cfg.profile.b = val;
    }
  } else {
    cfg.profile.r = DEFAULT_COLOR_R;
    cfg.profile.g = DEFAULT_COLOR_G;
    cfg.profile.b = DEFAULT_COLOR_B;
  }

  uint8_t val;
  if (nvs_get_u8(config_storage_handle, "bright", &val) == ESP_OK) {
    cfg.brightness = val;
  }

  uint8_t shareVal = 0;
  if (nvs_get_u8(config_storage_handle, "share", &shareVal) == ESP_OK) {
    cfg.share = shareVal != 0;
  }

  return cfg;
}

void configWrite(const UserConfig &cfg) {
  if (!is_nvs_initialized) {
    return;
  }

  nvs_set_str(config_storage_handle, "name", cfg.profile.name);
  nvs_set_str(config_storage_handle, "pronouns", cfg.profile.pronouns);
  nvs_set_str(config_storage_handle, "affil", cfg.profile.affiliation);
  nvs_set_str(config_storage_handle, "contact", cfg.profile.contact);
  nvs_set_u8(config_storage_handle, "r", cfg.profile.r);
  nvs_set_u8(config_storage_handle, "g", cfg.profile.g);
  nvs_set_u8(config_storage_handle, "b", cfg.profile.b);
  nvs_set_u8(config_storage_handle, COLOR_SET_KEY, 1);
  nvs_set_u8(config_storage_handle, "bright", cfg.brightness);
  nvs_set_u8(config_storage_handle, "share", cfg.share ? 1 : 0);
  nvs_commit(config_storage_handle);
}

}  // namespace storage
}  // namespace core
