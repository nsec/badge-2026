#pragma once

// ESP-IDF NVS key-value store stubs for native simulator.
// Uses an in-memory std::map so storage operations actually work.

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esp_partition.h"

#define ESP_ERR_NVS_NOT_FOUND 0x1102

typedef uint32_t nvs_handle_t;

typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

namespace _nvs_sim {

// Storage: namespace → key → blob.
inline std::map<std::string, std::map<std::string, std::vector<uint8_t>>> &store() {
  static std::map<std::string, std::map<std::string, std::vector<uint8_t>>> s;
  return s;
}

// Handle → namespace name mapping.
inline std::map<nvs_handle_t, std::string> &handles() {
  static std::map<nvs_handle_t, std::string> h;
  return h;
}

inline nvs_handle_t nextHandle() {
  static nvs_handle_t next = 1;
  return next++;
}

inline std::map<std::string, std::vector<uint8_t>> *ns(nvs_handle_t h) {
  auto it = handles().find(h);
  if (it == handles().end())
    return nullptr;
  return &store()[it->second];
}

}  // namespace _nvs_sim

inline esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t /*mode*/, nvs_handle_t *out_handle) {
  nvs_handle_t h = _nvs_sim::nextHandle();
  _nvs_sim::handles()[h] = namespace_name;
  *out_handle = h;
  return ESP_OK;
}

inline esp_err_t nvs_close(nvs_handle_t handle) {
  _nvs_sim::handles().erase(handle);
  return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t /*handle*/) {
  return ESP_OK;
}

// --- getters ---

inline esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  auto it = ns->find(key);
  if (it == ns->end())
    return ESP_ERR_NVS_NOT_FOUND;
  if (it->second.size() < 1)
    return ESP_FAIL;
  *out_value = it->second[0];
  return ESP_OK;
}

inline esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  auto it = ns->find(key);
  if (it == ns->end())
    return ESP_ERR_NVS_NOT_FOUND;
  size_t stored_len = it->second.size();
  if (out_value == nullptr) {
    // Query length.
    *length = stored_len;
    return ESP_OK;
  }
  if (*length < stored_len)
    return ESP_FAIL;
  std::memcpy(out_value, it->second.data(), stored_len);
  *length = stored_len;
  return ESP_OK;
}

inline esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  auto it = ns->find(key);
  if (it == ns->end())
    return ESP_ERR_NVS_NOT_FOUND;
  size_t stored_len = it->second.size();
  if (out_value == nullptr) {
    *length = stored_len;
    return ESP_OK;
  }
  if (*length < stored_len)
    return ESP_FAIL;
  std::memcpy(out_value, it->second.data(), stored_len);
  *length = stored_len;
  return ESP_OK;
}

// --- setters ---

inline esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  (*ns)[key] = {value};
  return ESP_OK;
}

inline esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  size_t len = std::strlen(value) + 1;  // include null terminator
  (*ns)[key] =
      std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(value), reinterpret_cast<const uint8_t *>(value) + len);
  return ESP_OK;
}

inline esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  const auto *p = reinterpret_cast<const uint8_t *>(value);
  (*ns)[key] = std::vector<uint8_t>(p, p + length);
  return ESP_OK;
}

inline esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
  auto *ns = _nvs_sim::ns(handle);
  if (!ns)
    return ESP_FAIL;
  ns->erase(key);
  return ESP_OK;
}
