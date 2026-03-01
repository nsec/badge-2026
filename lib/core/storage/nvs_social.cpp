#include "storage/nvs_social.h"

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_random.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hardware/hwid.h"

namespace {

nvs_handle_t g_nvsHandle = 0;
bool g_initialized = false;

// NVS key under which the encrypted blob is stored.
static constexpr const char *BLOB_KEY = "data";

// Maximum size of the payload (nonce + JSON, pre-encryption).
static constexpr size_t MAX_BLOB_SIZE = 128;

// Nonce length range (inclusive).
static constexpr size_t NONCE_MIN = 3;
static constexpr size_t NONCE_MAX = 7;

// ---------------------------------------------------------------------------
// Encryption primitives
// ---------------------------------------------------------------------------

/// Simple PRNG (xorshift32) used to generate the shuffle permutation.
/// Seeded per-badge from the MAC so the permutation is deterministic per badge.
struct Xorshift32 {
  uint32_t state;
  explicit Xorshift32(uint32_t seed) : state(seed ? seed : 1) {}
  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
};

/// Derive a 32-bit seed from the full MAC.
uint32_t macSeed() {
  uint8_t mac[core::hw::MAC_LEN];
  core::hw::getHwidMac(mac);
  // FNV-1a 32-bit hash of the 6 MAC bytes
  uint32_t h = 2166136261u;
  for (int i = 0; i < core::hw::MAC_LEN; i++) {
    h ^= mac[i];
    h *= 16777619u;
  }
  return h;
}

/// XOR `buf` in-place with the 6-byte MAC, cycling.
void xorWithMac(uint8_t *buf, size_t len) {
  uint8_t mac[core::hw::MAC_LEN];
  core::hw::getHwidMac(mac);
  for (size_t i = 0; i < len; i++) {
    buf[i] ^= mac[i % core::hw::MAC_LEN];
  }
}

/// Fisher-Yates shuffle of `buf` using a MAC-seeded PRNG.
void shuffleBytes(uint8_t *buf, size_t len) {
  if (len < 2)
    return;
  Xorshift32 rng(macSeed());
  for (size_t i = len - 1; i > 0; i--) {
    size_t j = rng.next() % (i + 1);
    uint8_t tmp = buf[i];
    buf[i] = buf[j];
    buf[j] = tmp;
  }
}

/// Reverse the Fisher-Yates shuffle (replay the same swaps in reverse order).
void unshuffleBytes(uint8_t *buf, size_t len) {
  if (len < 2)
    return;
  Xorshift32 rng(macSeed());
  // Record the swap indices
  size_t *indices = static_cast<size_t *>(alloca((len - 1) * sizeof(size_t)));
  for (size_t i = len - 1; i > 0; i--) {
    indices[len - 1 - i] = rng.next() % (i + 1);
  }
  // Replay in reverse
  for (size_t k = 0; k < len - 1; k++) {
    size_t i = k + 1;  // original i went from len-1 down to 1, reversed is 1 up to len-1
    size_t j = indices[(len - 1) - i];
    uint8_t tmp = buf[i];
    buf[i] = buf[j];
    buf[j] = tmp;
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
  return snprintf(buf, bufSize,
                  "{\"social\":%u,\"sponsor\":%u,\"light\":%u,\"attraction\":%u}",
                  d.social, d.sponsor, d.light, d.attraction);
}

uint8_t parseU8(const char *s, size_t len, size_t &pos) {
  while (pos < len && (s[pos] < '0' || s[pos] > '9'))
    pos++;
  int val = 0;
  while (pos < len && s[pos] >= '0' && s[pos] <= '9') {
    val = val * 10 + (s[pos] - '0');
    pos++;
  }
  return static_cast<uint8_t>(val > 254 ? 254 : val);
}

SocialData fromJson(const char *json, size_t len) {
  SocialData d;
  auto findKey = [&](const char *key) -> uint8_t {
    const char *p = strstr(json, key);
    if (!p)
      return 0;
    size_t pos = static_cast<size_t>((p - json) + strlen(key));
    while (pos < len && json[pos] != ':')
      pos++;
    if (pos < len)
      pos++;
    return parseU8(json, len, pos);
  };

  d.social = findKey("\"social\"");
  d.sponsor = findKey("\"sponsor\"");
  d.light = findKey("\"light\"");
  d.attraction = findKey("\"attraction\"");
  return d;
}

// ---------------------------------------------------------------------------
// Blob read / write
// ---------------------------------------------------------------------------

/// Encrypt: [nonce_len_byte | random_nonce | JSON] → XOR → shuffle
/// The first byte stores the nonce length (NONCE_MIN..NONCE_MAX).
/// nonce_len_byte itself is part of the XOR+shuffle, so it's not plaintext.

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

  // Reverse: unshuffle, then un-XOR
  unshuffleBytes(raw, blobLen);
  xorWithMac(raw, blobLen);

  // First byte is the nonce length
  uint8_t nonceLen = raw[0];
  if (nonceLen < NONCE_MIN || nonceLen > NONCE_MAX)
    return d;  // corrupted

  size_t headerLen = 1 + nonceLen;  // length byte + nonce bytes
  if (blobLen <= headerLen)
    return d;

  d = fromJson(reinterpret_cast<const char *>(raw + headerLen), blobLen - headerLen);
  return d;
}

void writeBlob(const SocialData &d) {
  if (!g_initialized)
    return;

  uint8_t payload[MAX_BLOB_SIZE];

  // Random nonce length (NONCE_MIN..NONCE_MAX)
  uint8_t nonceLen = NONCE_MIN + (esp_random() % (NONCE_MAX - NONCE_MIN + 1));

  // Byte 0: nonce length
  payload[0] = nonceLen;

  // Bytes 1..nonceLen: random nonce
  uint32_t rnd = esp_random();
  for (uint8_t i = 0; i < nonceLen; i++) {
    if (i % 4 == 0 && i > 0)
      rnd = esp_random();
    payload[1 + i] = static_cast<uint8_t>(rnd >> (8 * (i % 4)));
  }

  size_t headerLen = 1 + nonceLen;

  // Serialize JSON after header
  int jsonLen = toJson(d, reinterpret_cast<char *>(payload + headerLen),
                       sizeof(payload) - headerLen);
  if (jsonLen <= 0)
    return;

  size_t totalLen = headerLen + static_cast<size_t>(jsonLen);

  // Encrypt: XOR then shuffle
  xorWithMac(payload, totalLen);
  shuffleBytes(payload, totalLen);

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

}  // namespace storage
}  // namespace core
