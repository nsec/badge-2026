#include "storage/nvs_config.h"

#include <Arduino.h>
#include <nvs.h>
#include <cstring>

namespace {

nvs_handle_t config_storage_handle = 0;
bool is_nvs_initialized = false;

static constexpr const char *NS = "config";

}  // namespace

namespace core {
namespace storage {

void configNvsInit() {
  if (is_nvs_initialized)
    return;

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
  if (!is_nvs_initialized) {
    return cfg;
  }

  size_t len = sizeof(cfg.name);
  if (nvs_get_str(config_storage_handle, "name", cfg.name, &len) != ESP_OK)
    cfg.name[0] = '\0';

  uint8_t val;
  if (nvs_get_u8(config_storage_handle, "r", &val) == ESP_OK) {
    cfg.favoriteColor.r = val;
  }

  if (nvs_get_u8(config_storage_handle, "g", &val) == ESP_OK) {
    cfg.favoriteColor.g = val;
  }

  if (nvs_get_u8(config_storage_handle, "b", &val) == ESP_OK) {
    cfg.favoriteColor.b = val;
  }

  if (nvs_get_u8(config_storage_handle, "bright", &val) == ESP_OK) {
    cfg.brightness = val;
  }

  return cfg;
}

void configWrite(const UserConfig &cfg) {
  if (!is_nvs_initialized)
    return;

  nvs_set_str(config_storage_handle, "name", cfg.name);
  nvs_set_u8(config_storage_handle, "r", cfg.favoriteColor.r);
  nvs_set_u8(config_storage_handle, "g", cfg.favoriteColor.g);
  nvs_set_u8(config_storage_handle, "b", cfg.favoriteColor.b);
  nvs_set_u8(config_storage_handle, "bright", cfg.brightness);
  nvs_commit(config_storage_handle);
}

}  // namespace storage
}  // namespace core
