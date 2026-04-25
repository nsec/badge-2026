#pragma once

// ESP-IDF NVS flash init stubs for native simulator.

#include "esp_partition.h"

#define ESP_ERR_NVS_NO_FREE_PAGES     0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1101

inline esp_err_t nvs_flash_init() {
  return ESP_OK;
}

inline esp_err_t nvs_flash_erase() {
  return ESP_OK;
}
