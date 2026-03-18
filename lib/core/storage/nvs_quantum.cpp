#include "storage/nvs_quantum.h"

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace {

static constexpr const char *NVS_NAMESPACE = "quantum";

}  // namespace

namespace core {
namespace storage {

void quantumNvsInit() {
  // NVS flash is already initialized by socialNvsInit().
  // Just verify the namespace is accessible.
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
    nvs_close(h);
    Serial.println("NVS: 'quantum' namespace ready");
  }
}

uint8_t quantumRead(const char *key) {
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
    return 0;
  uint8_t val = 0;
  nvs_get_u8(h, key, &val);
  nvs_close(h);
  return val;
}

void quantumWrite(const char *key, uint8_t value) {
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
    return;
  nvs_set_u8(h, key, value);
  nvs_commit(h);
  nvs_close(h);
}

bool quantumReadStr(const char *key, char *buf, size_t bufSize) {
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
    return false;
  size_t len = bufSize;
  esp_err_t err = nvs_get_str(h, key, buf, &len);
  nvs_close(h);
  return (err == ESP_OK);
}

void quantumWriteStr(const char *key, const char *value) {
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
    return;
  nvs_set_str(h, key, value);
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace storage
}  // namespace core
