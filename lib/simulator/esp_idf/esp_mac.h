#pragma once

// ESP-IDF MAC address API stubs for native simulator

#include <cstdint>
#include <cstring>

typedef int esp_err_t;
#ifndef ESP_OK
  #define ESP_OK 0
#endif

inline esp_err_t esp_efuse_mac_get_default(uint8_t *mac) {
  // Return a fake MAC address for the simulator
  const uint8_t fake_mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
  memcpy(mac, fake_mac, 6);
  return ESP_OK;
}
