#include "storage/nvs_social.h"

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "hardware/hwid.h"

namespace {

nvs_handle_t g_nvsHandle = 0;
bool g_initialized = false;

// NVS key under which the encrypted blob is stored.
static constexpr const char *BLOB_KEY = "data";

// Maximum blob size (JSON + CRC, pre-encryption).
static constexpr size_t MAX_BLOB_SIZE = 128;

// ---------------------------------------------------------------------------
// Integrity check — CRC-8/CCITT (polynomial 0x07)
//
// The CRC is computed over the plaintext JSON and stored as the last byte
// of the blob (before encryption).  An attacker who dumps NVS and XOR's
// with the MAC gets the JSON, but to *modify* values they also need to
// recompute a valid CRC — which requires knowing the CRC algorithm, the
// polynomial, and that the CRC covers only the JSON portion.
// ---------------------------------------------------------------------------

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }
  return crc;
}

// ---------------------------------------------------------------------------
// XOR encryption — cycling 6-byte MAC
// ---------------------------------------------------------------------------

void xorWithMac(uint8_t *buf, size_t len) {
  uint8_t mac[core::hw::MAC_LEN];
  core::hw::getHwidMac(mac);
  for (size_t i = 0; i < len; i++) {
    buf[i] ^= mac[i % core::hw::MAC_LEN];
  }
}

// ---------------------------------------------------------------------------
// JSON serialization
// ---------------------------------------------------------------------------

struct SocialData {
  uint8_t social = 0;
  uint8_t sponsor = 0;
  uint8_t light = 0;
  uint8_t attraction = 0;
};

int toJson(const SocialData &d, char *buf, size_t bufSize) {
  return snprintf(buf, bufSize, "{\"social\":%u,\"sponsor\":%u,\"light\":%u,\"attraction\":%u}", d.social, d.sponsor,
                  d.light, d.attraction);
}

SocialData fromJson(std::string_view json) {
  SocialData d;
  auto findKey = [&](std::string_view key) -> uint8_t {
    auto pos = json.find(key);
    if (pos == std::string_view::npos)
      return 0;
    pos += key.size();
    pos = json.find(':', pos);
    if (pos == std::string_view::npos)
      return 0;
    pos++;
    while (pos < json.size() && (json[pos] < '0' || json[pos] > '9'))
      pos++;
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
      val = val * 10 + (json[pos] - '0');
      pos++;
    }
    return static_cast<uint8_t>(val > 255 ? 255 : val);
  };

  d.social = findKey("\"social\"");
  d.sponsor = findKey("\"sponsor\"");
  d.light = findKey("\"light\"");
  d.attraction = findKey("\"attraction\"");
  return d;
}

// ---------------------------------------------------------------------------
// Blob read / write
//
// Stored format (encrypted):  XOR( [JSON bytes | CRC-8], cycling MAC )
// The CRC-8 covers only the JSON bytes (plaintext), so tampering with the
// encrypted blob without knowing both the MAC and the CRC scheme will
// corrupt the checksum.
// ---------------------------------------------------------------------------

SocialData readBlob() {
  SocialData d;
  if (!g_initialized)
    return d;

  size_t blobLen = 0;
  esp_err_t err = nvs_get_blob(g_nvsHandle, BLOB_KEY, nullptr, &blobLen);
  if (err == ESP_ERR_NVS_NOT_FOUND || blobLen == 0)
    return d;
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    Serial.printf("NVS: read blob size failed (%d)\r\n", err);
    return d;
  }

  if (blobLen > MAX_BLOB_SIZE)
    blobLen = MAX_BLOB_SIZE;

  uint8_t raw[MAX_BLOB_SIZE];
  err = nvs_get_blob(g_nvsHandle, BLOB_KEY, raw, &blobLen);
  if (err != ESP_OK) {
    Serial.printf("NVS: read blob failed (%d)\r\n", err);
    return d;
  }

  // Decrypt
  xorWithMac(raw, blobLen);

  // Need at least 1 byte of JSON + 1 CRC byte
  if (blobLen < 2) {
    Serial.println("NVS: corrupted blob (too short), resetting social data");
    nvs_erase_key(g_nvsHandle, BLOB_KEY);
    nvs_commit(g_nvsHandle);
    return d;
  }

  // Last byte is CRC-8 over the JSON portion
  size_t jsonLen = blobLen - 1;
  uint8_t storedCrc = raw[blobLen - 1];
  uint8_t computedCrc = crc8(raw, jsonLen);

  if (storedCrc != computedCrc) {
    Serial.println("NVS: corrupted blob (CRC mismatch), resetting social data");
    nvs_erase_key(g_nvsHandle, BLOB_KEY);
    nvs_commit(g_nvsHandle);
    return d;
  }

  d = fromJson({reinterpret_cast<const char *>(raw), jsonLen});
  return d;
}

void writeBlob(const SocialData &d) {
  if (!g_initialized)
    return;

  uint8_t payload[MAX_BLOB_SIZE];

  // Serialize JSON
  int jsonLen = toJson(d, reinterpret_cast<char *>(payload),
                       sizeof(payload) - 1);  // -1 for CRC byte
  if (jsonLen <= 0)
    return;

  // Append CRC-8 over the plaintext JSON
  payload[jsonLen] = crc8(payload, static_cast<size_t>(jsonLen));

  size_t totalLen = static_cast<size_t>(jsonLen) + 1;  // JSON + CRC

  // Encrypt entire payload (JSON + CRC) with cycling MAC
  xorWithMac(payload, totalLen);

  esp_err_t err = nvs_set_blob(g_nvsHandle, BLOB_KEY, payload, totalLen);
  if (err != ESP_OK) {
    Serial.printf("NVS: write blob failed (%d)\r\n", err);
    return;
  }

  err = nvs_commit(g_nvsHandle);
  if (err != ESP_OK) {
    Serial.printf("NVS: commit failed (%d)\r\n", err);
  }
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
    Serial.println("NVS: erasing and re-initializing...");
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.printf("NVS: flash init failed (%d)\r\n", err);
    return;
  }

  err = nvs_open("social", NVS_READWRITE, &g_nvsHandle);
  if (err != ESP_OK) {
    Serial.printf("NVS: open 'social' namespace failed (%d)\r\n", err);
    return;
  }

  g_initialized = true;
  Serial.println("NVS: 'social' namespace ready");
}

uint8_t socialRead(SocialKey key) {
  SocialData d = readBlob();
  switch (key) {
    case SocialKey::Social:
      return d.social;
    case SocialKey::Sponsor:
      return d.sponsor;
    case SocialKey::Light:
      return d.light;
    case SocialKey::Attraction:
      return d.attraction;
    default:
      return 0;
  }
}

void socialWrite(SocialKey key, uint8_t value) {
  // Read-modify-write the whole blob
  SocialData d = readBlob();
  switch (key) {
    case SocialKey::Social:
      d.social = value;
      break;
    case SocialKey::Sponsor:
      d.sponsor = value;
      break;
    case SocialKey::Light:
      d.light = value;
      break;
    case SocialKey::Attraction:
      d.attraction = value;
      break;
    default:
      return;
  }
  writeBlob(d);
}

const char *socialKeyName(SocialKey key) {
  return keyStr(key);
}

// ---------------------------------------------------------------------------
// Pair partner tracking
// ---------------------------------------------------------------------------

uint16_t pairCount() {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READONLY, &h) != ESP_OK)
    return 0;
  size_t blobLen = 0;
  nvs_get_blob(h, "seen", nullptr, &blobLen);
  nvs_close(h);
  return static_cast<uint16_t>(blobLen / 6);
}

bool pairGet(uint16_t index, uint8_t mac[6]) {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READONLY, &h) != ESP_OK)
    return false;
  size_t blobLen = 0;
  nvs_get_blob(h, "seen", nullptr, &blobLen);
  uint16_t count = static_cast<uint16_t>(blobLen / 6);
  if (index >= count) {
    nvs_close(h);
    return false;
  }
  uint8_t *buf = static_cast<uint8_t *>(malloc(blobLen));
  if (!buf) {
    nvs_close(h);
    return false;
  }
  nvs_get_blob(h, "seen", buf, &blobLen);
  memcpy(mac, buf + index * 6, 6);
  free(buf);
  nvs_close(h);
  return true;
}

void pairReset() {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READWRITE, &h) != ESP_OK)
    return;
  nvs_erase_key(h, "seen");
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace storage
}  // namespace core
