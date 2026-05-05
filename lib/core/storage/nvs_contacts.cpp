#include "storage/nvs_contacts.h"

#include <Arduino.h>
#include <nvs.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <new>

namespace {

static constexpr const char *NS = "contacts";
static constexpr const char *KEY = "data";
static constexpr size_t MAC_LEN = core::storage::MacAddress{}.size();
using BlobPtr = std::unique_ptr<uint8_t[]>;

// All contacts are stored as a single NVS blob. Operations read the full blob
// into RAM, modify it, and write it back. This is fine given:
//   - Max ~130 bytes per contact (all fields filled), ~13 KB for 100 contacts
//   - ESP32-S3 has 512 KB SRAM; the blob is short-lived (freed within each call)
//   - NVS partition is 24 KB, so the blob can never exceed that
//
// Packed storage format per contact:
//   [6B mac] [1B name_len] [name] [1B pronouns_len] [pronouns]
//   [1B affil_len] [affil] [1B contact_len] [contact] [3B rgb]

size_t packedSize(const core::storage::ContactProfile &c) {
  return MAC_LEN + 1 + strlen(c.name) + 1 + strlen(c.pronouns) + 1 + strlen(c.affiliation) + 1 + strlen(c.contact) + 3;
}

size_t packContact(const core::storage::ContactProfile &c, uint8_t *buf) {
  size_t pos = 0;
  memcpy(buf + pos, c.mac.data(), MAC_LEN);
  pos += MAC_LEN;

  uint8_t len;

  len = static_cast<uint8_t>(strlen(c.name));
  buf[pos++] = len;
  memcpy(buf + pos, c.name, len);
  pos += len;

  len = static_cast<uint8_t>(strlen(c.pronouns));
  buf[pos++] = len;
  memcpy(buf + pos, c.pronouns, len);
  pos += len;

  len = static_cast<uint8_t>(strlen(c.affiliation));
  buf[pos++] = len;
  memcpy(buf + pos, c.affiliation, len);
  pos += len;

  len = static_cast<uint8_t>(strlen(c.contact));
  buf[pos++] = len;
  memcpy(buf + pos, c.contact, len);
  pos += len;

  buf[pos++] = c.r;
  buf[pos++] = c.g;
  buf[pos++] = c.b;

  return pos;
}

// Returns bytes consumed, or 0 on error.
size_t unpackContact(const uint8_t *buf, size_t available, core::storage::ContactProfile &out) {
  out = {};
  size_t pos = 0;

  if (available < MAC_LEN + 4 + 3) {
    return 0;  // minimum: mac + 4 length bytes + rgb
  }

  memcpy(out.mac.data(), buf + pos, MAC_LEN);
  pos += MAC_LEN;

  auto readField = [&](char *dst, uint8_t maxLen) -> bool {
    if (pos >= available) {
      return false;
    }
    uint8_t len = buf[pos++];
    if (pos + len > available || len > maxLen) {
      return false;
    }
    memcpy(dst, buf + pos, len);
    dst[len] = '\0';
    pos += len;
    return true;
  };

  if (!readField(out.name, badge::config::profile::name_max_len)) {
    return 0;
  }
  if (!readField(out.pronouns, badge::config::profile::pronouns_max_len)) {
    return 0;
  }
  if (!readField(out.affiliation, badge::config::profile::affiliation_max_len)) {
    return 0;
  }
  if (!readField(out.contact, badge::config::profile::contact_max_len)) {
    return 0;
  }

  if (pos + 3 > available) {
    return 0;
  }
  out.r = buf[pos++];
  out.g = buf[pos++];
  out.b = buf[pos++];

  return pos;
}

// Read the full contacts blob from NVS.
// Sets blobLen to the blob size. Returns null BlobPtr if empty or error.
BlobPtr readBlob(size_t &blobLen) {
  blobLen = 0;
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
    return nullptr;
  }

  nvs_get_blob(h, KEY, nullptr, &blobLen);
  if (blobLen == 0) {
    nvs_close(h);
    return nullptr;
  }

  BlobPtr buf(new (std::nothrow) uint8_t[blobLen]);
  if (!buf) {
    nvs_close(h);
    return nullptr;
  }

  if (nvs_get_blob(h, KEY, buf.get(), &blobLen) != ESP_OK) {
    nvs_close(h);
    return nullptr;
  }

  nvs_close(h);
  return buf;
}

}  // namespace

namespace core {
namespace storage {

void contactWrite(const ContactProfile &profile) {
  // Read existing blob, replace entry if MAC already exists, otherwise append.
  size_t blobLen = 0;
  BlobPtr blob = readBlob(blobLen);

  // Scan for existing entry with same MAC.
  size_t existingOffset = 0;
  size_t existingSize = 0;
  if (blob) {
    size_t pos = 0;
    while (pos < blobLen) {
      ContactProfile tmp;
      size_t consumed = unpackContact(blob.get() + pos, blobLen - pos, tmp);
      if (consumed == 0) {
        break;
      }

      if (tmp.mac == profile.mac) {
        existingOffset = pos;
        existingSize = consumed;
        break;
      }
      pos += consumed;
    }
  }

  size_t newEntrySize = packedSize(profile);
  size_t newBlobLen;

  if (existingSize > 0) {
    // Replace existing entry.
    newBlobLen = blobLen - existingSize + newEntrySize;
  } else {
    // Append.
    newBlobLen = blobLen + newEntrySize;
  }

  BlobPtr newBlob(new (std::nothrow) uint8_t[newBlobLen]);
  if (!newBlob) {
    return;
  }

  size_t writePos = 0;
  if (existingSize > 0) {
    // Copy everything before the existing entry.
    memcpy(newBlob.get(), blob.get(), existingOffset);
    writePos = existingOffset;
    // Pack the new entry.
    writePos += packContact(profile, newBlob.get() + writePos);
    // Copy everything after the existing entry.
    size_t after = existingOffset + existingSize;
    memcpy(newBlob.get() + writePos, blob.get() + after, blobLen - after);
  } else {
    // Copy existing blob, then append.
    if (blob) {
      memcpy(newBlob.get(), blob.get(), blobLen);
    }
    writePos = blobLen;
    packContact(profile, newBlob.get() + writePos);
  }

  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_blob(h, KEY, newBlob.get(), newBlobLen);
    nvs_commit(h);
    nvs_close(h);
  }
}

uint16_t contactCount() {
  size_t blobLen = 0;
  BlobPtr blob = readBlob(blobLen);
  if (!blob) {
    return 0;
  }

  uint16_t count = 0;
  size_t pos = 0;
  while (pos < blobLen) {
    ContactProfile tmp;
    size_t consumed = unpackContact(blob.get() + pos, blobLen - pos, tmp);
    if (consumed == 0) {
      break;
    }
    count++;
    pos += consumed;
  }

  return count;
}

bool contactGet(uint16_t index, ContactProfile &out) {
  size_t blobLen = 0;
  BlobPtr blob = readBlob(blobLen);
  if (!blob) {
    return false;
  }

  uint16_t i = 0;
  size_t pos = 0;
  while (pos < blobLen) {
    ContactProfile tmp;
    size_t consumed = unpackContact(blob.get() + pos, blobLen - pos, tmp);
    if (consumed == 0) {
      break;
    }

    if (i == index) {
      out = tmp;
      return true;
    }
    i++;
    pos += consumed;
  }

  return false;
}

bool contactDelete(const MacAddress &mac) {
  size_t blobLen = 0;
  BlobPtr blob = readBlob(blobLen);
  if (!blob) {
    return false;
  }

  // Find the entry matching this MAC.
  size_t pos = 0;
  bool found = false;
  size_t entryOffset = 0;
  size_t entrySize = 0;

  while (pos < blobLen) {
    ContactProfile tmp;
    size_t consumed = unpackContact(blob.get() + pos, blobLen - pos, tmp);
    if (consumed == 0) {
      break;
    }

    if (tmp.mac == mac) {
      entryOffset = pos;
      entrySize = consumed;
      found = true;
      break;
    }
    pos += consumed;
  }

  if (!found) {
    return false;
  }

  size_t newBlobLen = blobLen - entrySize;

  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
    return false;
  }

  if (newBlobLen == 0) {
    nvs_erase_key(h, KEY);
  } else {
    // Shift remaining data over the deleted entry.
    memmove(blob.get() + entryOffset, blob.get() + entryOffset + entrySize, blobLen - entryOffset - entrySize);
    nvs_set_blob(h, KEY, blob.get(), newBlobLen);
  }

  nvs_commit(h);
  nvs_close(h);
  return true;
}

void contactReset() {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
    return;
  }
  nvs_erase_key(h, KEY);
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace storage
}  // namespace core
