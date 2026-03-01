#pragma once

// ESP-IDF OTA API stubs for native simulator

#include "esp_partition.h"

// Track simulated boot state
static const esp_partition_t *g_running_partition = &g_factory_partition;
static const esp_partition_t *g_boot_partition = &g_factory_partition;

inline const esp_partition_t *esp_ota_get_running_partition() {
  return g_running_partition;
}

inline const esp_partition_t *esp_ota_get_boot_partition() {
  return g_boot_partition;
}

inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition) {
  if (partition == nullptr) {
    return ESP_FAIL;
  }
  g_boot_partition = partition;
  return ESP_OK;
}
