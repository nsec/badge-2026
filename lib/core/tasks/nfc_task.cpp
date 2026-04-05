#include "tasks/nfc_task.h"

#include <Arduino.h>
#include <cstring>
#include "hardware/serial_mutex.h"

#include <rfal_rf.h>
#include <rfal_nfc.h>
#include <rfal_nfca.h>
#include <rfal_nfcb.h>
#include <rfal_nfcv.h>
#include <rfal_nfcDep.h>
#include <rfal_t2t.h>
#include <rfal_rfst25r3916.h>
#include <st_errno.h>
#include <mbedtls/md.h>

#include "hardware/nfc.h"
#include "hardware/board_pins.h"
#include "hardware/crypto1.h"
#include "hardware/hwid.h"

#include <nvs_flash.h>
#include <nvs.h>

#include "storage/nvs_social.h"

namespace core {

Queue<NfcCommand> *g_nfcQueue = nullptr;

// ===========================================================================
// NTAG213 Emulation
// ===========================================================================

namespace {

#define NTAG213_PAGES      45
#define NTAG213_PAGE_SIZE  4
#define NTAG213_USER_START 4

static uint8_t tagMemory[NTAG213_PAGES * NTAG213_PAGE_SIZE];

static const uint8_t NTAG213_VERSION[] = {
    0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x0F, 0x03,
};

// NDEF message is built dynamically in initTagMemory() with the badge's MAC
static uint8_t g_ndefBuf[64];
static uint8_t g_ndefLen = 0;

// UID is derived from the badge's MAC: 0x04 + 6 MAC bytes = 7-byte NTAG UID
static uint8_t g_tagUid[7];

#define TX_BUF_LEN (NTAG213_PAGES * NTAG213_PAGE_SIZE)
#define RX_BUF_LEN 64
static uint8_t g_emuTxBuf[TX_BUF_LEN];
static uint8_t g_emuRxBuf[RX_BUF_LEN];
static uint16_t g_emuRxRcvdLen = 0;
static rfalTransceiveContext g_trxCtx;
static rfalLmConfPA g_lmConfigA;
static uint32_t g_lmConfigMask;
static bool g_isFirstFrame = true;
static bool g_wasEverActivated = false;
static uint32_t g_lastActivityMs = 0;
#define STUCK_TIMEOUT_MS 1000

// ===========================================================================
// NFC-DEP Pair Protocol - crypto helpers
// ===========================================================================

// Firmware-wide secret for per-badge key derivation.
// CTF players can extract this, but the NFC-DEP protocol + HMAC
// challenge-response makes spoofing non-trivial regardless.
static const uint8_t BADGE_SECRET[32] = {
    0x4E, 0x53, 0x45, 0x43, 0x32, 0x30, 0x32, 0x36, 0x42, 0x41, 0x44, 0x47, 0x45, 0x5F, 0x50, 0x41,
    0x49, 0x52, 0x5F, 0x53, 0x45, 0x43, 0x52, 0x45, 0x54, 0x5F, 0x4B, 0x45, 0x59, 0x21, 0x21, 0x21,
};

#define PAIR_NONCE_LEN 16
#define PAIR_HMAC_LEN  32
// DEP payload: MAC(6) + HMAC(32) = 38 bytes
#define PAIR_DEP_LEN (core::hw::MAC_LEN + PAIR_HMAC_LEN)

/// Derive a per-badge key: HMAC-SHA256(BADGE_SECRET, mac)
static void deriveKey(const uint8_t mac[core::hw::MAC_LEN], uint8_t keyOut[32]) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), BADGE_SECRET, sizeof(BADGE_SECRET), mac,
                  core::hw::MAC_LEN, keyOut);
}

/// Compute proof: HMAC-SHA256(key, nonce_partner || mac_partner)
static void computeProof(const uint8_t key[32], const uint8_t nonce[PAIR_NONCE_LEN],
                         const uint8_t mac[core::hw::MAC_LEN], uint8_t hmacOut[PAIR_HMAC_LEN]) {
  uint8_t msg[PAIR_NONCE_LEN + core::hw::MAC_LEN];
  memcpy(msg, nonce, PAIR_NONCE_LEN);
  memcpy(msg + PAIR_NONCE_LEN, mac, core::hw::MAC_LEN);
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 32, msg, sizeof(msg), hmacOut);
}

// ===========================================================================
// Partner tracking - NVS-based unique pair tracking
// ===========================================================================

// Store seen partner MACs as a blob of 6-byte entries in NVS.
// Max 128 partners (768 bytes blob). Each entry is a raw 6-byte MAC.
#define MAX_PAIRED_PARTNERS 128

/// Check if a partner MAC has been seen before. Returns true if new (not seen).
static bool isNewPartner(const uint8_t mac[core::hw::MAC_LEN]) {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READONLY, &h) != ESP_OK)
    return true;  // namespace doesn't exist yet â†’ definitely new

  size_t blobLen = 0;
  esp_err_t err = nvs_get_blob(h, "seen", nullptr, &blobLen);
  if (err != ESP_OK || blobLen == 0) {
    nvs_close(h);
    return true;
  }

  uint8_t *buf = static_cast<uint8_t *>(malloc(blobLen));
  if (!buf) {
    nvs_close(h);
    return true;
  }

  nvs_get_blob(h, "seen", buf, &blobLen);
  nvs_close(h);

  size_t count = blobLen / core::hw::MAC_LEN;
  for (size_t i = 0; i < count; i++) {
    if (memcmp(buf + i * core::hw::MAC_LEN, mac, core::hw::MAC_LEN) == 0) {
      free(buf);
      return false;  // already seen
    }
  }
  free(buf);
  return true;  // new partner
}

/// Record a partner MAC as seen.
static void recordPartner(const uint8_t mac[core::hw::MAC_LEN]) {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READWRITE, &h) != ESP_OK)
    return;

  size_t blobLen = 0;
  nvs_get_blob(h, "seen", nullptr, &blobLen);

  size_t count = blobLen / core::hw::MAC_LEN;
  if (count >= MAX_PAIRED_PARTNERS) {
    nvs_close(h);
    return;  // storage full
  }

  size_t newLen = blobLen + core::hw::MAC_LEN;
  uint8_t *buf = static_cast<uint8_t *>(malloc(newLen));
  if (!buf) {
    nvs_close(h);
    return;
  }

  if (blobLen > 0)
    nvs_get_blob(h, "seen", buf, &blobLen);

  memcpy(buf + blobLen, mac, core::hw::MAC_LEN);
  nvs_set_blob(h, "seen", buf, newLen);
  nvs_commit(h);
  nvs_close(h);
  free(buf);
}

/// Get the count of unique partners seen.
static uint16_t getPartnerCount() {
  nvs_handle_t h;
  if (nvs_open("pairs", NVS_READONLY, &h) != ESP_OK)
    return 0;

  size_t blobLen = 0;
  nvs_get_blob(h, "seen", nullptr, &blobLen);
  nvs_close(h);
  return static_cast<uint16_t>(blobLen / core::hw::MAC_LEN);
}

/// Fill buffer with random bytes from hardware RNG
static void fillRandom(uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i += 4) {
    uint32_t r = esp_random();
    size_t n = (len - i < 4) ? (len - i) : 4;
    memcpy(buf + i, &r, n);
  }
}

// ---------------------------------------------------------------------------
// Custom NDEF text — stored in NVS namespace "ndef"
// ---------------------------------------------------------------------------

static char g_customNdefText[128] = {};  // empty = use default

static void loadCustomNdef() {
  nvs_handle_t h;
  if (nvs_open("ndef", NVS_READONLY, &h) != ESP_OK)
    return;
  size_t len = sizeof(g_customNdefText) - 1;
  if (nvs_get_str(h, "text", g_customNdefText, &len) != ESP_OK)
    g_customNdefText[0] = '\0';
  nvs_close(h);
}

void initTagMemory() {
  // Load custom NDEF text from NVS (if any)
  loadCustomNdef();

  // Build unique UID from MAC: 0x04 (NXP) + 6 MAC bytes
  uint8_t mac[core::hw::MAC_LEN];
  core::hw::getHwidMac(mac);
  g_tagUid[0] = 0x04;  // NXP manufacturer code (required for NTAG)
  memcpy(&g_tagUid[1], mac, 6);

  memset(tagMemory, 0x00, sizeof(tagMemory));
  tagMemory[0] = g_tagUid[0];
  tagMemory[1] = g_tagUid[1];
  tagMemory[2] = g_tagUid[2];
  tagMemory[3] = g_tagUid[0] ^ g_tagUid[1] ^ g_tagUid[2] ^ 0x88;
  tagMemory[4] = g_tagUid[3];
  tagMemory[5] = g_tagUid[4];
  tagMemory[6] = g_tagUid[5];
  tagMemory[7] = g_tagUid[6];
  tagMemory[8] = g_tagUid[3] ^ g_tagUid[4] ^ g_tagUid[5] ^ g_tagUid[6];
  tagMemory[9] = 0x48;
  tagMemory[12] = 0xE1;
  tagMemory[13] = 0x10;
  tagMemory[14] = 0x12;
  tagMemory[15] = 0x00;

  // Build NDEF Text record
  // Use custom text if set in NVS, otherwise default to "NSEC Badge <MAC>"
  char macHex[13];
  snprintf(macHex, sizeof(macHex), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char ndefText[128];
  if (g_customNdefText[0] != '\0') {
    strncpy(ndefText, g_customNdefText, sizeof(ndefText) - 1);
    ndefText[sizeof(ndefText) - 1] = '\0';
  } else {
    snprintf(ndefText, sizeof(ndefText), "NSEC Badge %s", macHex);
  }
  uint8_t textLen = static_cast<uint8_t>(strlen(ndefText));
  if (textLen > 100)
    textLen = 100;                           // keep within NTAG213 capacity
  uint8_t textPayloadLen = 1 + 2 + textLen;  // status + "en" + text

  // NDEF record header (SR=1, MB=1, ME=1, TNF=0x01 well-known, type='T')
  uint8_t ndefRecord[128];
  uint8_t pos = 0;
  ndefRecord[pos++] = 0xD1;            // MB|ME|SR, TNF=0x01
  ndefRecord[pos++] = 0x01;            // type length = 1
  ndefRecord[pos++] = textPayloadLen;  // payload length
  ndefRecord[pos++] = 'T';             // type = Text
  ndefRecord[pos++] = 0x02;            // status: UTF-8, lang len = 2
  ndefRecord[pos++] = 'e';
  ndefRecord[pos++] = 'n';
  memcpy(&ndefRecord[pos], ndefText, textLen);
  pos += textLen;

  // Build TLV: 0x03 <len> <record> 0xFE
  g_ndefLen = 0;
  g_ndefBuf[g_ndefLen++] = 0x03;  // NDEF TLV type
  g_ndefBuf[g_ndefLen++] = pos;   // NDEF record length
  memcpy(&g_ndefBuf[g_ndefLen], ndefRecord, pos);
  g_ndefLen += pos;
  g_ndefBuf[g_ndefLen++] = 0xFE;  // Terminator TLV

  memcpy(&tagMemory[NTAG213_USER_START * NTAG213_PAGE_SIZE], g_ndefBuf, g_ndefLen);

  tagMemory[41 * 4] = 0x04;
  tagMemory[41 * 4 + 3] = 0xFF;
  tagMemory[43 * 4] = tagMemory[43 * 4 + 1] = tagMemory[43 * 4 + 2] = tagMemory[43 * 4 + 3] = 0xFF;

  // core::hw::safeSerial().printf("NFC emu: NDEF text = \"%s%s\"\r\n", prefix, macHex);
  // core::hw::safeSerial().printf("NFC emu: UID = %02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", g_tagUid[0], g_tagUid[1],
  // g_tagUid[2],
  //               g_tagUid[3], g_tagUid[4], g_tagUid[5], g_tagUid[6]);
}

uint16_t handleNtagCommand(const uint8_t *cmd, uint16_t cmdLen, uint8_t *resp) {
  if (cmdLen < 1)
    return 0;
  switch (cmd[0]) {
    case 0x30: {  // READ
      if (cmdLen < 2)
        return 0;
      uint8_t page = cmd[1];
      if (page >= NTAG213_PAGES)
        return 0;
      for (int i = 0; i < 4; i++)
        memcpy(&resp[i * 4], &tagMemory[((page + i) % NTAG213_PAGES) * NTAG213_PAGE_SIZE], NTAG213_PAGE_SIZE);
      return 16;
    }
    case 0x3A: {  // FAST_READ
      if (cmdLen < 3)
        return 0;
      uint8_t s = cmd[1], e = cmd[2];
      if (s > e || e >= NTAG213_PAGES)
        return 0;
      uint16_t len = (e - s + 1) * NTAG213_PAGE_SIZE;
      if (len > TX_BUF_LEN)
        len = TX_BUF_LEN;
      memcpy(resp, &tagMemory[s * NTAG213_PAGE_SIZE], len);
      return len;
    }
    case 0x60:  // GET_VERSION
      if (cmdLen != 1)
        return 0;
      memcpy(resp, NTAG213_VERSION, sizeof(NTAG213_VERSION));
      return sizeof(NTAG213_VERSION);
    case 0x1A:  // PWD_AUTH
      resp[0] = tagMemory[44 * 4];
      resp[1] = tagMemory[44 * 4 + 1];
      return 2;
    case 0x3C:  // READ_SIG
      memset(resp, 0, 32);
      return 32;
    case 0x39:  // READ_CNT
      resp[0] = resp[1] = resp[2] = 0;
      return 3;
    default:
      return 0;
  }
}

void restartListen(RfalRfST25R3916Class &hw) {
  hw.rfalListenStop();
  hw.rfalListenStart(g_lmConfigMask, &g_lmConfigA, NULL, NULL, g_emuRxBuf, rfalConvBytesToBits(RX_BUF_LEN),
                     &g_emuRxRcvdLen);
  g_isFirstFrame = true;
  g_lastActivityMs = millis();
}

bool sendEmuResponse(RfalRfST25R3916Class &hw, const uint8_t *data, uint16_t lenBytes) {
  memcpy(g_trxCtx.txBuf, data, lenBytes);
  g_trxCtx.txBufLen = rfalConvBytesToBits(lenBytes);
  *g_trxCtx.rxRcvdLen = 0;
  g_trxCtx.flags = RFAL_TXRX_FLAGS_DEFAULT;
  ReturnCode err = hw.rfalStartTransceive(&g_trxCtx);
  if (err == ERR_NONE) {
    g_isFirstFrame = false;
    return true;
  }
  return false;
}

void processEmuFrame(RfalRfST25R3916Class &hw, const uint8_t *buf, uint16_t lenBytes) {
  if (lenBytes == 2 && buf[0] == 0x50 && buf[1] == 0x00) {
    hw.rfalListenSleepStart(RFAL_LM_STATE_SLEEP_A, g_emuRxBuf, RX_BUF_LEN, &g_emuRxRcvdLen);
    g_isFirstFrame = true;
    digitalWrite(badge::pins::NFC_LED, LOW);
    return;
  }

  uint16_t txLen = handleNtagCommand(buf, lenBytes, g_emuTxBuf);
  if (txLen > 0) {
    digitalWrite(badge::pins::NFC_LED, HIGH);
    if (!sendEmuResponse(hw, g_emuTxBuf, txLen))
      restartListen(hw);
  } else {
    hw.rfalListenSleepStart(RFAL_LM_STATE_SLEEP_A, g_emuRxBuf, RX_BUF_LEN, &g_emuRxRcvdLen);
    g_isFirstFrame = true;
    digitalWrite(badge::pins::NFC_LED, LOW);
  }
}

// ===========================================================================
// Reader: NDEF parsing
// ===========================================================================

static const char *uriPrefixes[] = {
    "",
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "tel:",
    "mailto:",
    "ftp://anonymous:anonymous@",
    "ftp://ftp.",
    "ftps://",
    "sftp://",
    "smb://",
    "nfs://",
    "ftp://",
    "dav://",
    "news:",
    "telnet://",
    "imap:",
    "rtsp://",
    "urn:",
    "pop:",
    "sip:",
    "sips:",
    "tftp:",
    "btspp://",
    "btl2cap://",
    "btgoep://",
    "tcpobex://",
    "irdaobex://",
    "file://",
    "urn:epc:id:",
    "urn:epc:tag:",
    "urn:epc:pat:",
    "urn:epc:raw:",
    "urn:epc:",
    "urn:nfc:",
};

void parseNdefMessage(const uint8_t *data, uint16_t len) {
  uint16_t pos = 0;
  while (pos < len) {
    uint8_t header = data[pos++];
    if (pos >= len)
      break;
    bool me = header & 0x40, sr = header & 0x10, il = header & 0x08;
    uint8_t tnf = header & 0x07;
    uint8_t typeLen = data[pos++];
    uint32_t payloadLen = sr ? data[pos++]
                             : (((uint32_t)data[pos] << 24) | ((uint32_t)data[pos + 1] << 16) |
                                ((uint32_t)data[pos + 2] << 8) | data[pos + 3]);
    if (!sr)
      pos += 4;
    uint8_t idLen = il ? data[pos++] : 0;
    if (pos + typeLen + idLen + payloadLen > len)
      break;
    const uint8_t *type = &data[pos];
    pos += typeLen + idLen;
    const uint8_t *payload = &data[pos];
    pos += payloadLen;

    if (tnf == 0x01 && typeLen == 1 && type[0] == 'U' && payloadLen > 0) {
      core::hw::safeSerial().print("  NDEF URI: ");
      uint8_t code = payload[0];
      if (code < sizeof(uriPrefixes) / sizeof(uriPrefixes[0]))
        core::hw::safeSerial().print(uriPrefixes[code]);
      for (uint32_t i = 1; i < payloadLen; i++)
        core::hw::safeSerial().print((char)payload[i]);
      core::hw::safeSerial().println();
    } else if (tnf == 0x01 && typeLen == 1 && type[0] == 'T' && payloadLen > 0) {
      uint8_t langLen = payload[0] & 0x3F;
      core::hw::safeSerial().print("  NDEF Text: ");
      for (uint32_t i = 1 + langLen; i < payloadLen; i++)
        core::hw::safeSerial().print((char)payload[i]);
      core::hw::safeSerial().println();
    } else {
      core::hw::safeSerial().printf("  NDEF record TNF=%d payload=%d bytes\r\n", tnf, payloadLen);
    }
    if (me)
      break;
  }
}

void readNtag(RfalNfcClass &nfc) {
  uint8_t rxBuf[RFAL_T2T_READ_DATA_LEN];
  uint16_t rcvLen;
  core::hw::safeSerial().println("Reading NTAG...");
  ReturnCode err = nfc.rfalT2TPollerRead(0, rxBuf, sizeof(rxBuf), &rcvLen);
  if (err != ERR_NONE) {
    core::hw::safeSerial().printf("Read page 0 failed: %d\r\n", err);
    return;
  }
  if (rxBuf[12] != 0xE1) {
    core::hw::safeSerial().printf("Not NDEF (CC=0x%02X)\r\n", rxBuf[12]);
    return;
  }
  uint16_t totalBytes = (uint16_t)rxBuf[14] * 8;
  core::hw::safeSerial().printf("NDEF tag, %d bytes\r\n", totalBytes);

  uint8_t ndefBuf[256];
  uint16_t ndefLen = 0;
  uint8_t maxPages = (totalBytes + 3) / 4;
  if (maxPages > 60)
    maxPages = 60;
  for (uint8_t pg = 4; pg < 4 + maxPages; pg += 4) {
    err = nfc.rfalT2TPollerRead(pg, rxBuf, sizeof(rxBuf), &rcvLen);
    if (err != ERR_NONE)
      break;
    uint16_t toCopy = rcvLen;
    if (ndefLen + toCopy > sizeof(ndefBuf))
      toCopy = sizeof(ndefBuf) - ndefLen;
    memcpy(&ndefBuf[ndefLen], rxBuf, toCopy);
    ndefLen += toCopy;
  }
  uint16_t pos = 0;
  while (pos < ndefLen) {
    uint8_t t = ndefBuf[pos++];
    if (t == 0x00)
      continue;
    if (t == 0xFE)
      break;
    if (pos >= ndefLen)
      break;
    uint16_t tl = (ndefBuf[pos] == 0xFF) ? (((uint16_t)ndefBuf[pos + 1] << 8) | ndefBuf[pos + 2]) : ndefBuf[pos];
    pos += (ndefBuf[pos] == 0xFF) ? 3 : 1;
    if (t == 0x03 && pos + tl <= ndefLen)
      parseNdefMessage(&ndefBuf[pos], tl);
    pos += tl;
  }
}

// ===========================================================================
// Reader: Mifare Classic
// ===========================================================================

static const uint8_t MFC_KEYS[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};
#define MFC_NUM_KEYS (sizeof(MFC_KEYS) / sizeof(MFC_KEYS[0]))

void crc14443a(const uint8_t *d, uint8_t len, uint8_t *a, uint8_t *b) {
  uint32_t w = 0x6363;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t bt = d[i] ^ (w & 0xFF);
    bt ^= bt << 4;
    w = (w >> 8) ^ ((uint32_t)bt << 8) ^ ((uint32_t)bt << 3) ^ ((uint32_t)bt >> 4);
  }
  *a = w & 0xFF;
  *b = (w >> 8) & 0xFF;
}

uint16_t packWithParity(const uint8_t *data, const uint8_t *par, uint8_t n, uint8_t *out) {
  uint16_t total = (uint16_t)n * 9;
  memset(out, 0, (total + 7) / 8);
  uint16_t bp = 0;
  for (uint8_t i = 0; i < n; i++) {
    for (int b = 0; b < 8; b++) {
      if (data[i] & (1 << b))
        out[bp / 8] |= (1 << (bp % 8));
      bp++;
    }
    if (par[i] & 1)
      out[bp / 8] |= (1 << (bp % 8));
    bp++;
  }
  return total;
}

void unpackWithParity(const uint8_t *raw, uint16_t bits, uint8_t *data, uint8_t *par, uint8_t max) {
  uint16_t bp = 0;
  uint8_t bi = 0;
  while (bp + 9 <= bits && bi < max) {
    data[bi] = 0;
    for (int b = 0; b < 8; b++) {
      if (raw[bp / 8] & (1 << (bp % 8)))
        data[bi] |= (1 << b);
      bp++;
    }
    par[bi] = (raw[bp / 8] >> (bp % 8)) & 1;
    bp++;
    bi++;
  }
}

ReturnCode transceiveRaw(uint8_t *tx, uint16_t txBits, uint8_t *rx, uint16_t rxBufBits, uint16_t *rxBits,
                         uint32_t flags, uint32_t fwt) {
  RfalRfST25R3916Class &hw = hw::nfcHardware();
  rfalTransceiveContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.txBuf = tx;
  ctx.txBufLen = txBits;
  ctx.rxBuf = rx;
  ctx.rxBufLen = rxBufBits;
  ctx.rxRcvdLen = rxBits;
  ctx.flags = flags;
  ctx.fwt = fwt;
  ReturnCode err = hw.rfalStartTransceive(&ctx);
  if (err != ERR_NONE)
    return err;
  do {
    hw.rfalWorker();
    err = hw.rfalGetTransceiveStatus();
  } while (err == ERR_BUSY);
  return err;
}

ReturnCode directTransceiveRaw(uint8_t *tx, uint16_t txBits, uint8_t *rx, uint16_t rxBytes, uint16_t *rxBits) {
  RfalRfST25R3916Class &hw = hw::nfcHardware();
  hw.st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
  hw.st25r3916SetRegisterBits(ST25R3916_REG_ISO14443A_NFC,
                              ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par);
  hw.st25r3916SetRegisterBits(ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
  hw.st25r3916SetNumTxBits(txBits);
  hw.st25r3916WriteFifo(tx, (txBits + 7) / 8);
  uint32_t mask = ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_RXS | ST25R3916_IRQ_MASK_NRE |
                  ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC | ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_FWL;
  hw.st25r3916GetInterrupt(mask);
  hw.st25r3916EnableInterrupts(mask);
  hw.st25r3916ChangeRegisterBits(ST25R3916_REG_TIMER_EMV_CONTROL,
                                 ST25R3916_REG_TIMER_EMV_CONTROL_nrt_step | ST25R3916_REG_TIMER_EMV_CONTROL_nrt_nfc,
                                 ST25R3916_REG_TIMER_EMV_CONTROL_nrt_step_4096_fc |
                                     ST25R3916_REG_TIMER_EMV_CONTROL_nrt_nfc);
  hw.st25r3916WriteRegister(ST25R3916_REG_NO_RESPONSE_TIMER1, 0x40);
  hw.st25r3916WriteRegister(ST25R3916_REG_NO_RESPONSE_TIMER2, 0xA9);
  hw.st25r3916ExecuteCommand(ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
  uint32_t irqs = hw.st25r3916WaitForInterruptsTimed(ST25R3916_IRQ_MASK_TXE, 20);
  if (!(irqs & ST25R3916_IRQ_MASK_TXE)) {
    hw.st25r3916ClrRegisterBits(ST25R3916_REG_ISO14443A_NFC,
                                ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par);
    return ERR_TIMEOUT;
  }
  irqs = hw.st25r3916WaitForInterruptsTimed(ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_NRE | ST25R3916_IRQ_MASK_CRC |
                                                ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_ERR1,
                                            50);
  ReturnCode ret;
  if (irqs & ST25R3916_IRQ_MASK_RXE) {
    uint8_t f1, f2;
    hw.st25r3916ReadRegister(ST25R3916_REG_FIFO_STATUS1, &f1);
    hw.st25r3916ReadRegister(ST25R3916_REG_FIFO_STATUS2, &f2);
    uint16_t rl = f1;
    uint8_t ib = (f2 >> 1) & 7;
    if (rl > 0 && rl <= rxBytes)
      hw.st25r3916ReadFifo(rx, rl);
    *rxBits = rl * 8 - (ib ? (8 - ib) : 0);
    ret = (irqs & ST25R3916_IRQ_MASK_CRC) ? ERR_CRC : ERR_NONE;
  } else if (irqs & ST25R3916_IRQ_MASK_NRE) {
    ret = ERR_TIMEOUT;
    *rxBits = 0;
  } else {
    ret = ERR_FRAMING;
    *rxBits = 0;
  }
  hw.st25r3916ClrRegisterBits(ST25R3916_REG_ISO14443A_NFC,
                              ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par);
  hw.st25r3916DisableInterrupts(mask);
  return ret;
}

uint8_t oddParity(uint8_t x) {
  x ^= x >> 4;
  x ^= x >> 2;
  x ^= x >> 1;
  return (~x) & 1;
}

bool mfcAuth(Crypto1 *c, uint8_t blk, const uint8_t key[6], const uint8_t *uid4) {
  uint8_t tb[4], rb[8];
  uint16_t rxb = 0;
  tb[0] = 0x60;
  tb[1] = blk;
  transceiveRaw(tb, rfalConvBytesToBits(2), rb, rfalConvBytesToBits(4), &rxb,
                (uint32_t)RFAL_TXRX_FLAGS_CRC_TX_AUTO | (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                    (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_MANUAL | (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_AUTO |
                    (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_REMV,
                rfalConvMsTo1fc(10));
  if (rxb < 32)
    return false;
  uint32_t nT = ((uint32_t)rb[0] << 24) | ((uint32_t)rb[1] << 16) | ((uint32_t)rb[2] << 8) | rb[3];
  uint32_t uid32 = ((uint32_t)uid4[0] << 24) | ((uint32_t)uid4[1] << 16) | ((uint32_t)uid4[2] << 8) | uid4[3];
  crypto1_init(c, key);
  crypto1_word(c, uid32 ^ nT, false);
  uint32_t nR = esp_random(), aR = prng_successor(nT, 64);
  uint8_t pl[8], ed[8], ep[8];
  pl[0] = (nR >> 24);
  pl[1] = (nR >> 16);
  pl[2] = (nR >> 8);
  pl[3] = nR;
  pl[4] = (aR >> 24);
  pl[5] = (aR >> 16);
  pl[6] = (aR >> 8);
  pl[7] = aR;
  for (int i = 0; i < 4; i++) {
    uint8_t ks = crypto1_byte(c, pl[i], false);
    ed[i] = pl[i] ^ ks;
    ep[i] = oddParity(pl[i]) ^ crypto1_peek(c);
  }
  for (int i = 4; i < 8; i++) {
    uint8_t ks = crypto1_byte(c, 0, false);
    ed[i] = pl[i] ^ ks;
    ep[i] = oddParity(pl[i]) ^ crypto1_peek(c);
  }
  uint8_t ptx[10], prx[8];
  uint16_t txb = packWithParity(ed, ep, 8, ptx), rxbe = 0;
  if (directTransceiveRaw(ptx, txb, prx, 8, &rxbe) != ERR_NONE)
    return false;
  uint8_t ad[4], ap[4];
  unpackWithParity(prx, rxbe, ad, ap, 4);
  for (int i = 0; i < 4; i++)
    ad[i] ^= crypto1_byte(c, 0, false);
  uint32_t aT = ((uint32_t)ad[0] << 24) | ((uint32_t)ad[1] << 16) | ((uint32_t)ad[2] << 8) | ad[3];
  return aT == prng_successor(nT, 96);
}

bool mfcReadBlock(Crypto1 *c, uint8_t blk, uint8_t *out) {
  uint8_t pl[4];
  pl[0] = 0x30;
  pl[1] = blk;
  crc14443a(pl, 2, &pl[2], &pl[3]);
  uint8_t ed[4], ep[4];
  for (int i = 0; i < 4; i++) {
    uint8_t ks = crypto1_byte(c, 0, false);
    ed[i] = pl[i] ^ ks;
    ep[i] = oddParity(pl[i]) ^ crypto1_peek(c);
  }
  uint8_t ptx[6];
  uint16_t txb = packWithParity(ed, ep, 4, ptx);
  uint8_t prx[24];
  uint16_t rxb = 0;
  if (directTransceiveRaw(ptx, txb, prx, sizeof(prx), &rxb) != ERR_NONE && rxb == 0)
    return false;
  uint8_t rd[18], rp[18];
  unpackWithParity(prx, rxb, rd, rp, 18);
  for (int i = 0; i < 18; i++)
    rd[i] ^= crypto1_byte(c, 0, false);
  uint8_t ca, cb;
  crc14443a(rd, 16, &ca, &cb);
  if (rd[16] != ca || rd[17] != cb)
    return false;
  memcpy(out, rd, 16);
  return true;
}

void mfcHalt(Crypto1 *c) {
  uint8_t pl[4];
  pl[0] = 0x50;
  pl[1] = 0x00;
  crc14443a(pl, 2, &pl[2], &pl[3]);
  uint8_t ed[4], ep[4];
  for (int i = 0; i < 4; i++) {
    ed[i] = pl[i] ^ crypto1_byte(c, 0, false);
    ep[i] = oddParity(pl[i]) ^ crypto1_peek(c);
  }
  uint8_t ptx[6];
  uint16_t txb = packWithParity(ed, ep, 4, ptx);
  uint8_t prx[4];
  uint16_t rxb = 0;
  directTransceiveRaw(ptx, txb, prx, sizeof(prx), &rxb);
}

bool mfcReselect(RfalNfcClass &nfc, rfalNfcDevice *dev, Crypto1 *c) {
  mfcHalt(c);
  nfc.rfalNfcaPollerInitialize();
  rfalNfcaSensRes sr;
  if (nfc.rfalNfcaPollerCheckPresence(RFAL_14443A_SHORTFRAME_CMD_WUPA, &sr) != ERR_NONE)
    return false;
  rfalNfcaSelRes sel;
  return nfc.rfalNfcaPollerSelect(dev->dev.nfca.nfcId1, dev->dev.nfca.nfcId1Len, &sel) == ERR_NONE;
}

void readMifareClassic(RfalNfcClass &nfc, rfalNfcDevice *dev) {
  uint8_t sak = dev->dev.nfca.selRes.sak;
  uint8_t ns = (sak == 0x18) ? 40 : 16;
  core::hw::safeSerial().printf("Mifare Classic %s\r\n", (sak == 0x18) ? "4K" : "1K");
  uint8_t uid4[4];
  memcpy(uid4, dev->dev.nfca.nfcId1, 4);
  Crypto1 crypto;
  for (uint8_t sec = 0; sec < ns && sec < 16; sec++) {
    vTaskDelay(pdMS_TO_TICKS(1));  // yield to prevent watchdog
    uint8_t fb = sec * 4;
    bool authed = false;
    if (sec > 0 && !mfcReselect(nfc, dev, &crypto)) {
      core::hw::safeSerial().printf("Sector %2d: lost\r\n", sec);
      break;
    }
    for (uint8_t ki = 0; ki < MFC_NUM_KEYS; ki++) {
      if (mfcAuth(&crypto, fb, MFC_KEYS[ki], uid4)) {
        authed = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));  // yield between key attempts
      if (!mfcReselect(nfc, dev, &crypto))
        break;
    }
    if (!authed) {
      core::hw::safeSerial().printf("Sector %2d: no key\r\n", sec);
      continue;
    }
    uint8_t bd[16];
    for (uint8_t b = 0; b < 4; b++) {
      if (mfcReadBlock(&crypto, fb + b, bd)) {
        core::hw::safeSerial().printf("  S%02d B%02d: ", sec, fb + b);
        for (int i = 0; i < 16; i++)
          core::hw::safeSerial().printf("%02X ", bd[i]);
        core::hw::safeSerial().println();
      }
    }
  }
}

// ===========================================================================
// State helpers
// ===========================================================================

static void stopAndFlush(RfalNfcClass &nfc) {
  nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);
  for (int i = 0; i < 50; i++) {
    nfc.rfalNfcWorker();
    if (nfc.rfalNfcGetState() == RFAL_NFC_STATE_IDLE || nfc.rfalNfcGetState() == RFAL_NFC_STATE_NOTINIT)
      break;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static bool ensureReady(RfalNfcClass &nfc) {
  // Always re-initialize - the emulator may have reconfigured the chip
  ReturnCode err = nfc.rfalNfcInitialize();
  if (err != ERR_NONE) {
    core::hw::safeSerial().printf("NFC: rfalNfcInitialize failed (%d)\r\n", err);
    return false;
  }
  return (nfc.rfalNfcGetState() == RFAL_NFC_STATE_IDLE);
}

}  // namespace

// ===========================================================================
// NDEF text get/set/reset — accessible from CLI
// ===========================================================================

const char *ndefGetText() {
  return g_customNdefText[0] != '\0' ? g_customNdefText : nullptr;
}

bool ndefSetText(const char *text) {
  if (!text || strlen(text) == 0 || strlen(text) > 100)
    return false;

  nvs_handle_t h;
  if (nvs_open("ndef", NVS_READWRITE, &h) != ESP_OK)
    return false;
  nvs_set_str(h, "text", text);
  nvs_commit(h);
  nvs_close(h);

  strncpy(g_customNdefText, text, sizeof(g_customNdefText) - 1);
  g_customNdefText[sizeof(g_customNdefText) - 1] = '\0';

  // Rebuild tag memory with new text
  initTagMemory();
  return true;
}

void ndefReset() {
  nvs_handle_t h;
  if (nvs_open("ndef", NVS_READWRITE, &h) == ESP_OK) {
    nvs_erase_key(h, "text");
    nvs_commit(h);
    nvs_close(h);
  }
  g_customNdefText[0] = '\0';

  // Rebuild tag memory with default text
  initTagMemory();
}

// ===========================================================================
// NfcTask public interface
// ===========================================================================

bool NfcTask::checkCommand(NfcCommand &out) {
  return _nfcQueue.receive(out, Milliseconds(0));
}

void NfcTask::run() {
  initTagMemory();
  NfcMode cur = NfcMode::Off;
  NfcCommand cmd;
  for (;;) {
    if (!_nfcQueue.receive(cmd))
      continue;
    if (cmd.mode == cur && cur != NfcMode::Off) {
      if (cur == NfcMode::Reader)
        stopAndFlush(hw::nfcInstance());
      cur = NfcMode::Off;
      digitalWrite(badge::pins::NFC_LED, LOW);
      _ledQueue.send(LedCommand(LedCommandType::Off), Milliseconds(0));
      core::hw::safeSerial().print("> ");
      continue;
    }
    if (cur == NfcMode::Reader)
      stopAndFlush(hw::nfcInstance());
    cur = cmd.mode;
    if (cmd.mode == NfcMode::Reader) {
      core::hw::safeSerial().println("NFC: === READER ===");
      runReader();
    } else if (cmd.mode == NfcMode::Emulator) {
      core::hw::safeSerial().println("NFC: === EMULATOR ===");
      runEmulator();
    } else if (cmd.mode == NfcMode::Pair) {
      core::hw::safeSerial().println("NFC: === PAIR ===");
      runPair();
    }
    cur = NfcMode::Off;
  }
}

void NfcTask::runReader() {
  RfalNfcClass &nfc = hw::nfcInstance();
  if (!ensureReady(nfc)) {
    core::hw::safeSerial().println("NFC reader: init failed");
    return;
  }

  rfalNfcDiscoverParam dp;
  memset(&dp, 0, sizeof(dp));
  dp.compMode = RFAL_COMPLIANCE_MODE_NFC;
  dp.devLimit = 1;
  dp.nfcfBR = RFAL_BR_212;
  dp.ap2pBR = RFAL_BR_424;
  dp.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
  dp.GBLen = RFAL_NFCDEP_GB_MAX_LEN;
  dp.totalDuration = 1000U;
  dp.wakeupEnabled = false;
  dp.wakeupConfigDefault = true;
  dp.notifyCb = nullptr;
  if (nfc.rfalNfcDiscover(&dp) != ERR_NONE) {
    core::hw::safeSerial().println("NFC reader: discover failed");
    return;
  }
  core::hw::safeSerial().println("NFC reader: scanning... (press A to stop)");
  _ledQueue.send(LedCommand(LedCommandType::SolidWhite), Milliseconds(0));

  for (;;) {
    NfcCommand cmd;
    if (checkCommand(cmd)) {
      stopAndFlush(nfc);
      _ledQueue.send(LedCommand(LedCommandType::Off), Milliseconds(0));
      if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Reader)
        _nfcQueue.send(cmd, Milliseconds(0));
      core::hw::safeSerial().print("> ");
      return;
    }
    nfc.rfalNfcWorker();
    if (nfc.rfalNfcGetState() != RFAL_NFC_STATE_ACTIVATED) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    rfalNfcDevice *dev = nullptr;
    nfc.rfalNfcGetActiveDevice(&dev);
    if (!dev) {
      nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
      continue;
    }

    digitalWrite(badge::pins::NFC_LED, HIGH);
    if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA) {
      uint8_t sak = dev->dev.nfca.selRes.sak;
      core::hw::safeSerial().print("NFC-A UID: ");
      for (uint8_t i = 0; i < dev->dev.nfca.nfcId1Len; i++)
        core::hw::safeSerial().printf("%02X ", dev->dev.nfca.nfcId1[i]);
      core::hw::safeSerial().printf("SAK:0x%02X\r\n", sak);
      if (sak == 0x08 || sak == 0x18)
        readMifareClassic(nfc, dev);
      else if ((sak & 0x60) == 0x00)
        readNtag(nfc);
    } else if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCB) {
      core::hw::safeSerial().print("NFC-B UID: ");
      for (uint8_t i = 0; i < dev->nfcidLen; i++)
        core::hw::safeSerial().printf("%02X ", dev->nfcid[i]);
      core::hw::safeSerial().println();
    } else if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCV) {
      core::hw::safeSerial().print("NFC-V UID: ");
      for (uint8_t i = 0; i < dev->nfcidLen; i++)
        core::hw::safeSerial().printf("%02X ", dev->nfcid[i]);
      core::hw::safeSerial().println();
    }

    LedCommand lc{};
    lc.type = LedCommandType::ProgressFlash;
    lc.pixelCount = 18;
    lc.g = 255;
    _ledQueue.send(lc, Milliseconds(0));
    digitalWrite(badge::pins::NFC_LED, LOW);
    stopAndFlush(nfc);
    core::hw::safeSerial().println("---");
    core::hw::safeSerial().print("> ");
    core::hw::safeSerial().flush();
    return;
  }
}

void NfcTask::runEmulator() {
  RfalRfST25R3916Class &hw = hw::nfcHardware();
  ReturnCode err = hw.rfalInitialize();
  if (err != ERR_NONE) {
    core::hw::safeSerial().printf("NFC emu: init failed (%d)\r\n", err);
    return;
  }

  memset(&g_lmConfigA, 0, sizeof(g_lmConfigA));
  g_lmConfigA.nfcidLen = RFAL_LM_NFCID_LEN_07;
  memcpy(g_lmConfigA.nfcid, g_tagUid, sizeof(g_tagUid));
  g_lmConfigA.SENS_RES[0] = 0x44;
  g_lmConfigA.SENS_RES[1] = 0x00;
  g_lmConfigA.SEL_RES = 0x00;
  g_lmConfigMask = RFAL_LM_MASK_NFCA;

  g_trxCtx.txBuf = g_emuTxBuf;
  g_trxCtx.txBufLen = 0;
  g_trxCtx.rxBuf = g_emuRxBuf;
  g_trxCtx.rxBufLen = rfalConvBytesToBits(RX_BUF_LEN);
  g_trxCtx.rxRcvdLen = &g_emuRxRcvdLen;
  g_trxCtx.flags = RFAL_TXRX_FLAGS_DEFAULT;
  g_trxCtx.fwt = RFAL_FWT_NONE;

  err = hw.rfalListenStart(g_lmConfigMask, &g_lmConfigA, NULL, NULL, g_emuRxBuf, rfalConvBytesToBits(RX_BUF_LEN),
                           &g_emuRxRcvdLen);
  if (err != ERR_NONE) {
    core::hw::safeSerial().printf("NFC emu: listen failed (%d)\r\n", err);
    return;
  }

  core::hw::safeSerial().println("NFC emulator: NTAG213 (press B to stop)");
  g_isFirstFrame = true;
  g_wasEverActivated = false;
  g_lastActivityMs = millis();

  LedCommand lc(LedCommandType::SolidCyan);
  _ledQueue.send(lc, Milliseconds(0));

  for (;;) {
    NfcCommand cmd;
    if (checkCommand(cmd)) {
      hw.rfalListenStop();
      digitalWrite(badge::pins::NFC_LED, LOW);
      _ledQueue.send(LedCommand(LedCommandType::Off), Milliseconds(0));
      if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Emulator)
        _nfcQueue.send(cmd, Milliseconds(0));
      core::hw::safeSerial().println("NFC emu: stopped");
      core::hw::safeSerial().print("> ");
      core::hw::safeSerial().flush();
      return;
    }

    hw.rfalWorker();

    if (g_isFirstFrame) {
      bool dataFlag = false;
      rfalLmState lmSt = hw.rfalListenGetState(&dataFlag, NULL);
      if ((lmSt == RFAL_LM_STATE_ACTIVE_A || lmSt == RFAL_LM_STATE_ACTIVE_Ax) && dataFlag) {
        g_lastActivityMs = millis();
        g_wasEverActivated = true;
        uint16_t cl = rfalConvBitsToBytes(g_emuRxRcvdLen);
        if (cl > 0)
          processEmuFrame(hw, g_emuRxBuf, cl);
        else
          restartListen(hw);
      } else if (g_wasEverActivated && millis() - g_lastActivityMs > STUCK_TIMEOUT_MS) {
        restartListen(hw);
      }
    } else {
      ReturnCode te = hw.rfalGetTransceiveStatus();
      if (te == ERR_NONE) {
        g_lastActivityMs = millis();
        uint16_t cl = rfalConvBitsToBytes(*g_trxCtx.rxRcvdLen);
        if (cl > 0)
          processEmuFrame(hw, g_trxCtx.rxBuf, cl);
      } else if (te == ERR_LINK_LOSS || te != ERR_BUSY) {
        digitalWrite(badge::pins::NFC_LED, LOW);
        restartListen(hw);
      } else if (millis() - g_lastActivityMs > STUCK_TIMEOUT_MS) {
        digitalWrite(badge::pins::NFC_LED, LOW);
        restartListen(hw);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void NfcTask::runPair() {
  RfalNfcClass &nfc = hw::nfcInstance();

  // Prepare our identity
  uint8_t myMac[core::hw::MAC_LEN];
  core::hw::getHwidMac(myMac);

  // Derive our per-badge key
  uint8_t myKey[32];
  deriveKey(myMac, myKey);

  // Generate a fresh nonce for this session
  uint8_t myNonce[PAIR_NONCE_LEN];
  fillRandom(myNonce, sizeof(myNonce));

  core::hw::safeSerial().println("NFC-DEP pair: searching... (press A/B to stop)");
  _ledQueue.send(LedCommand(LedCommandType::SolidOrange), Milliseconds(0));

  // Break symmetry with true hardware RNG - different on every attempt
  bool preferPoll = (esp_random() & 0x01) != 0;
  uint32_t initialDelay = esp_random() % 2000U;
  core::hw::safeSerial().printf("NFC-DEP pair: delay %lums, role=%s\r\n", initialDelay, preferPoll ? "poll" : "listen");
  vTaskDelay(pdMS_TO_TICKS(initialDelay));

  for (;;) {
    NfcCommand cmd;
    if (checkCommand(cmd)) {
      _ledQueue.send(LedCommand(LedCommandType::Off), Milliseconds(0));
      if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Pair)
        _nfcQueue.send(cmd, Milliseconds(0));
      core::hw::safeSerial().println("NFC-DEP pair: cancelled");
      core::hw::safeSerial().print("> ");
      return;
    }

    // Re-init for each discovery cycle
    if (!ensureReady(nfc)) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    rfalNfcDiscoverParam dp;
    memset(&dp, 0, sizeof(dp));
    dp.compMode = RFAL_COMPLIANCE_MODE_NFC;
    dp.devLimit = 1;
    dp.nfcfBR = RFAL_BR_212;
    dp.ap2pBR = RFAL_BR_424;

    // Both modes poll+listen, but with very different timing
    dp.techs2Find = RFAL_NFC_POLL_TECH_AP2P | RFAL_NFC_LISTEN_TECH_AP2P;
    if (preferPoll) {
      dp.totalDuration = 50U;  // Minimal listen - rapid-fire ATR_REQ (~20/sec)
    } else {
      dp.totalDuration = 3000U;  // Long listen window to catch partner's ATR
    }

    // General Bytes: our nonce for ATR exchange
    memcpy(dp.GB, myNonce, PAIR_NONCE_LEN);
    dp.GBLen = PAIR_NONCE_LEN;

    // NFCID3: MAC + zeros
    memcpy(dp.nfcid3, myMac, core::hw::MAC_LEN);
    memset(dp.nfcid3 + core::hw::MAC_LEN, 0, RFAL_NFCDEP_NFCID3_LEN - core::hw::MAC_LEN);

    dp.nfcDepLR = RFAL_NFCDEP_LR_254;
    dp.wakeupEnabled = false;
    dp.wakeupConfigDefault = true;
    dp.notifyCb = nullptr;

    ReturnCode err = nfc.rfalNfcDiscover(&dp);
    if (err != ERR_NONE) {
      core::hw::safeSerial().printf("NFC-DEP pair: discover failed (%d)\r\n", err);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Run discovery loop until activated or timeout
    uint32_t cycleTimeout = preferPoll ? 500U : 4000U;
    uint32_t startMs = millis();
    bool activated = false;

    while (millis() - startMs < cycleTimeout) {
      if (checkCommand(cmd)) {
        stopAndFlush(nfc);
        _ledQueue.send(LedCommand(LedCommandType::Off), Milliseconds(0));
        if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Pair)
          _nfcQueue.send(cmd, Milliseconds(0));
        core::hw::safeSerial().println("NFC-DEP pair: cancelled");
        core::hw::safeSerial().print("> ");
        return;
      }

      nfc.rfalNfcWorker();
      if (nfc.rfalNfcGetState() == RFAL_NFC_STATE_ACTIVATED) {
        activated = true;
        break;
      }
      // Tight polling - NFC-DEP AP2P requires fast ISR processing
      taskYIELD();
    }

    if (!activated) {
      stopAndFlush(nfc);
      // Flip role preference for next cycle
      preferPoll = !preferPoll;
      // Longer random delay to desynchronize
      vTaskDelay(pdMS_TO_TICKS(100 + (esp_random() % 700)));
      continue;
    }

    // === NFC-DEP ACTIVATED ===
    rfalNfcDevice *dev = nullptr;
    nfc.rfalNfcGetActiveDevice(&dev);
    if (!dev || dev->rfInterface != RFAL_NFC_INTERFACE_NFCDEP) {
      core::hw::safeSerial().println("NFC-DEP pair: activated but not NFC-DEP");
      stopAndFlush(nfc);
      continue;
    }

    // Determine our role and extract partner's nonce from ATR General Bytes
    bool weAreInitiator = (dev->type == RFAL_NFC_LISTEN_TYPE_AP2P);
    const char *role = weAreInitiator ? "initiator" : "target";

    uint8_t partnerNonce[PAIR_NONCE_LEN];
    uint8_t partnerGBLen = dev->proto.nfcDep.info.GBLen;

    if (partnerGBLen < PAIR_NONCE_LEN) {
      core::hw::safeSerial().printf("NFC-DEP pair: partner GB too short (%d)\r\n", partnerGBLen);
      stopAndFlush(nfc);
      continue;
    }

    if (weAreInitiator) {
      memcpy(partnerNonce, dev->proto.nfcDep.activation.Target.ATR_RES.GBt, PAIR_NONCE_LEN);
    } else {
      memcpy(partnerNonce, dev->proto.nfcDep.activation.Initiator.ATR_REQ.GBi, PAIR_NONCE_LEN);
    }

    core::hw::safeSerial().printf("NFC-DEP pair: activated as %s\r\n", role);
    digitalWrite(badge::pins::NFC_LED, HIGH);

    // Build our DEP payload: MAC(6) + HMAC(32)
    // Proof = HMAC-SHA256(myKey, partnerNonce || myMac)
    // This proves we know the firmware secret + our identity
    uint8_t txPayload[PAIR_DEP_LEN];
    memcpy(txPayload, myMac, core::hw::MAC_LEN);
    computeProof(myKey, partnerNonce, myMac, txPayload + core::hw::MAC_LEN);

    // NFC-DEP data exchange (asymmetric: initiator sends first, target receives first)
    rfalNfcDepBufFormat txBuf;
    uint8_t *rxData = nullptr;
    uint16_t *rvdLen = nullptr;
    bool depDone = false;
    uint32_t depStart;

    memcpy(txBuf.inf, txPayload, PAIR_DEP_LEN);

    if (weAreInitiator) {
      // Initiator: send our payload in DEP_REQ, receive partner's in DEP_RES
      err = nfc.rfalNfcDataExchangeStart(txBuf.inf, PAIR_DEP_LEN, &rxData, &rvdLen, RFAL_FWT_NONE);
    } else {
      // Target: must receive first (txLen=0), then respond
      err = nfc.rfalNfcDataExchangeStart(nullptr, 0, &rxData, &rvdLen, RFAL_FWT_NONE);
    }

    if (err != ERR_NONE) {
      core::hw::safeSerial().printf("NFC-DEP pair: data exchange start failed (%d)\r\n", err);
      stopAndFlush(nfc);
      continue;
    }

    // Poll for first exchange completion
    depStart = millis();
    while (millis() - depStart < 5000) {
      nfc.rfalNfcWorker();
      err = nfc.rfalNfcDataExchangeGetStatus();
      if (err == ERR_NONE) {
        depDone = true;
        break;
      }
      if (err != ERR_BUSY && err != ERR_AGAIN) {
        core::hw::safeSerial().printf("NFC-DEP pair: data exchange error (%d)\r\n", err);
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (!depDone) {
      core::hw::safeSerial().println("NFC-DEP pair: first exchange failed");
      stopAndFlush(nfc);
      continue;
    }

    // If target: we received initiator's data, now send our response
    if (!weAreInitiator) {
      // Save received data before starting response (buffer may be reused)
      uint16_t rxLen = (rvdLen != nullptr) ? *rvdLen : 0;
      uint8_t rxCopy[PAIR_DEP_LEN];
      if (rxLen >= PAIR_DEP_LEN && rxData != nullptr) {
        memcpy(rxCopy, rxData, PAIR_DEP_LEN);
      } else {
        core::hw::safeSerial().printf("NFC-DEP pair: initiator payload too short (%d)\r\n", rxLen);
        stopAndFlush(nfc);
        continue;
      }

      // Send our response
      rxData = nullptr;
      rvdLen = nullptr;
      err = nfc.rfalNfcDataExchangeStart(txBuf.inf, PAIR_DEP_LEN, &rxData, &rvdLen, RFAL_FWT_NONE);
      if (err != ERR_NONE) {
        core::hw::safeSerial().printf("NFC-DEP pair: target response start failed (%d)\r\n", err);
        stopAndFlush(nfc);
        continue;
      }

      depDone = false;
      depStart = millis();
      while (millis() - depStart < 3000) {
        nfc.rfalNfcWorker();
        err = nfc.rfalNfcDataExchangeGetStatus();
        if (err == ERR_NONE || err == ERR_RELEASE_REQ) {
          // ERR_RELEASE_REQ is expected: initiator received our DEP_RES and sent RELEASE
          depDone = true;
          break;
        }
        if (err != ERR_BUSY && err != ERR_AGAIN) {
          core::hw::safeSerial().printf("NFC-DEP pair: target response error (%d)\r\n", err);
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }

      if (!depDone) {
        core::hw::safeSerial().println("NFC-DEP pair: target response failed");
        stopAndFlush(nfc);
        continue;
      }

      // Use saved copy of initiator's data
      rxData = rxCopy;
    }

    uint16_t finalRxLen = (weAreInitiator && rvdLen != nullptr) ? *rvdLen : PAIR_DEP_LEN;
    if (finalRxLen < PAIR_DEP_LEN) {
      core::hw::safeSerial().printf("NFC-DEP pair: partner payload too short (%d)\r\n", finalRxLen);
      stopAndFlush(nfc);
      continue;
    }

    // Parse partner's payload
    uint8_t partnerMac[core::hw::MAC_LEN];
    uint8_t partnerHmac[PAIR_HMAC_LEN];
    memcpy(partnerMac, rxData, core::hw::MAC_LEN);
    memcpy(partnerHmac, rxData + core::hw::MAC_LEN, PAIR_HMAC_LEN);

    // Verify: derive partner's key, recompute expected HMAC
    uint8_t partnerKey[32];
    deriveKey(partnerMac, partnerKey);

    uint8_t expectedHmac[PAIR_HMAC_LEN];
    computeProof(partnerKey, myNonce, partnerMac, expectedHmac);

    bool verified = (memcmp(partnerHmac, expectedHmac, PAIR_HMAC_LEN) == 0);

    // Clean session teardown
    nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);
    digitalWrite(badge::pins::NFC_LED, LOW);

    if (verified) {
      bool isNew = isNewPartner(partnerMac);

      if (isNew) {
        recordPartner(partnerMac);
        // Increment social NVS by 3 (capped at 255)
        uint8_t current = storage::socialRead(storage::SocialKey::Social);
        uint16_t newVal = (uint16_t)current + 3;
        if (newVal > 255)
          newVal = 255;
        storage::socialWrite(storage::SocialKey::Social, (uint8_t)newVal);

        core::hw::safeSerial().printf(
            "NFC-DEP pair: NEW partner %02X:%02X:%02X:%02X:%02X:%02X (%s) - social=%d (+3)\r\n", partnerMac[0],
            partnerMac[1], partnerMac[2], partnerMac[3], partnerMac[4], partnerMac[5], role, newVal);

        // Green flash - new partner
        LedCommand sc{};
        sc.type = LedCommandType::ProgressFlash;
        sc.pixelCount = 18;
        sc.g = 255;
        _ledQueue.send(sc, Milliseconds(0));
      } else {
        core::hw::safeSerial().printf("NFC-DEP pair: ALREADY SEEN partner %02X:%02X:%02X:%02X:%02X:%02X (%s)\r\n",
                                      partnerMac[0], partnerMac[1], partnerMac[2], partnerMac[3], partnerMac[4],
                                      partnerMac[5], role);

        // Yellow flash - already paired before
        LedCommand sc{};
        sc.type = LedCommandType::ProgressFlash;
        sc.pixelCount = 18;
        sc.r = 255;
        sc.g = 255;
        _ledQueue.send(sc, Milliseconds(0));
      }
    } else {
      core::hw::safeSerial().printf("NFC-DEP pair: HMAC FAILED - partner %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                                    partnerMac[0], partnerMac[1], partnerMac[2], partnerMac[3], partnerMac[4],
                                    partnerMac[5]);
      LedCommand sc{};
      sc.type = LedCommandType::ProgressFlash;
      sc.pixelCount = 18;
      sc.r = 255;
      _ledQueue.send(sc, Milliseconds(0));
    }

    // Regenerate nonce for next session
    fillRandom(myNonce, sizeof(myNonce));

    core::hw::safeSerial().print("> ");
    return;
  }
}

}  // namespace core
