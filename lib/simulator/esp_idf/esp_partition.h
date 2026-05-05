#pragma once

// ESP-IDF partition API stubs for native simulator

#include <cstdint>
#include <cstring>

// Error codes
typedef int esp_err_t;
#define ESP_OK            0
#define ESP_FAIL          -1
#define ESP_ERR_NOT_FOUND 0x105

// Partition types
typedef enum {
  ESP_PARTITION_TYPE_APP = 0x00,
  ESP_PARTITION_TYPE_DATA = 0x01,
} esp_partition_type_t;

typedef enum {
  ESP_PARTITION_SUBTYPE_ANY = 0xff,
  ESP_PARTITION_SUBTYPE_APP_FACTORY = 0x00,
  ESP_PARTITION_SUBTYPE_APP_OTA_MIN = 0x10,
  ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10,
  ESP_PARTITION_SUBTYPE_APP_OTA_1 = 0x11,
  ESP_PARTITION_SUBTYPE_APP_OTA_MAX = 0x1f,
} esp_partition_subtype_t;

typedef struct {
  esp_partition_type_t type;
  esp_partition_subtype_t subtype;
  uint32_t address;
  uint32_t size;
  char label[17];
  bool encrypted;
} esp_partition_t;

// Simulated partitions
static const esp_partition_t g_factory_partition = {
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, 0x10000, 0x140000, "factory", false};

static const esp_partition_t g_ota_0_partition = {
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x150000, 0x140000, "ota_0", false};

// Find partition by label
inline const esp_partition_t *esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype,
                                                       const char *label) {
  (void)type;
  (void)subtype;

  if (label) {
    if (strcmp(label, "factory") == 0) {
      return &g_factory_partition;
    }
    if (strcmp(label, "ota_0") == 0) {
      return &g_ota_0_partition;
    }
  }
  return nullptr;
}
