#include "tasks/nfc_task.h"

#include <Arduino.h>
#include <cstring>

#include <rfal_rf.h>
#include <rfal_nfc.h>
#include <rfal_nfca.h>
#include <rfal_nfcb.h>
#include <rfal_nfcv.h>
#include <rfal_isoDep.h>
#include <st_errno.h>

#include "hardware/nfc.h"
#include "hardware/board_pins.h"

namespace core {

Queue<NfcCommand> *g_nfcQueue = nullptr;

// ---------------------------------------------------------------------------
// Tag emulation data (NDEF Type 4 Tag — text record "NSec Badge")
// ---------------------------------------------------------------------------

namespace {

// NDEF message
static const uint8_t NDEF_MESSAGE[] = {
    0xD1, 0x01, 0x0E, 0x54,              // MB|ME|SR, type len, payload len, 'T'
    0x02, 0x65, 0x6E,                     // UTF-8, "en"
    0x4E, 0x53, 0x65, 0x63, 0x20,        // "NSec "
    0x42, 0x61, 0x64, 0x67, 0x65         // "Badge"
};

// Capability Container
static const uint8_t CC_FILE[] = {
    0x00, 0x0F, 0x20, 0x00, 0x3B, 0x00, 0x34,
    0x04, 0x06, 0xE1, 0x04, 0x00, 0x32, 0x00, 0xFF,
};

static uint8_t g_ndefFile[50];
static uint16_t g_ndefFileLen;

static const uint8_t NDEF_APP_AID[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
static const uint8_t CC_FILE_ID[]   = {0xE1, 0x03};
static const uint8_t NDEF_FILE_ID[] = {0xE1, 0x04};

enum TagState { TAG_IDLE, TAG_APP_SELECTED, TAG_CC_SELECTED, TAG_NDEF_SELECTED };
static TagState g_tagState = TAG_IDLE;

static uint8_t g_txBuf[64];

uint16_t handleApdu(const uint8_t *rx, uint16_t rxLen, uint8_t *tx) {
  if (rxLen < 4) {
    tx[0] = 0x6A; tx[1] = 0x82;
    return 2;
  }

  uint8_t ins = rx[1];
  uint8_t p1  = rx[2];
  uint8_t p2  = rx[3];
  uint8_t lc  = (rxLen > 4) ? rx[4] : 0;

  if (ins == 0xA4) {
    if (p1 == 0x04 && p2 == 0x00 && lc == sizeof(NDEF_APP_AID) &&
        memcmp(&rx[5], NDEF_APP_AID, sizeof(NDEF_APP_AID)) == 0) {
      g_tagState = TAG_APP_SELECTED;
      tx[0] = 0x90; tx[1] = 0x00;
      return 2;
    }
    if (p1 == 0x00 && p2 == 0x0C && lc >= 2) {
      if (memcmp(&rx[5], CC_FILE_ID, 2) == 0) {
        g_tagState = TAG_CC_SELECTED;
        tx[0] = 0x90; tx[1] = 0x00;
        return 2;
      }
      if (memcmp(&rx[5], NDEF_FILE_ID, 2) == 0) {
        g_tagState = TAG_NDEF_SELECTED;
        tx[0] = 0x90; tx[1] = 0x00;
        return 2;
      }
    }
    tx[0] = 0x6A; tx[1] = 0x82;
    return 2;

  } else if (ins == 0xB0) {
    uint16_t offset = ((uint16_t)p1 << 8) | p2;
    uint8_t le = (rxLen > 4) ? rx[4] : 0;

    const uint8_t *fileData = nullptr;
    uint16_t fileSize = 0;
    if (g_tagState == TAG_CC_SELECTED) {
      fileData = CC_FILE;
      fileSize = sizeof(CC_FILE);
    } else if (g_tagState == TAG_NDEF_SELECTED) {
      fileData = g_ndefFile;
      fileSize = g_ndefFileLen;
    }

    if (fileData && offset < fileSize) {
      uint16_t available = fileSize - offset;
      uint16_t toRead = (le > 0 && le < available) ? le : available;
      if (toRead > 50) toRead = 50;
      memcpy(tx, fileData + offset, toRead);
      tx[toRead] = 0x90;
      tx[toRead + 1] = 0x00;
      return toRead + 2;
    }
    tx[0] = 0x6A; tx[1] = 0x00;
    return 2;

  } else {
    tx[0] = 0x6D; tx[1] = 0x00;
    return 2;
  }
}

void buildNdefFile() {
  uint16_t msgLen = sizeof(NDEF_MESSAGE);
  g_ndefFile[0] = (uint8_t)(msgLen >> 8);
  g_ndefFile[1] = (uint8_t)(msgLen & 0xFF);
  memcpy(g_ndefFile + 2, NDEF_MESSAGE, msgLen);
  g_ndefFileLen = 2 + msgLen;
}

}  // namespace

// ---------------------------------------------------------------------------
// NfcTask implementation
// ---------------------------------------------------------------------------

bool NfcTask::checkCommand(NfcCommand &out) {
  return _nfcQueue.receive(out, Milliseconds(0));
}

static const char *nfcStateName(rfalNfcState st) {
  switch (st) {
    case RFAL_NFC_STATE_NOTINIT:            return "NOTINIT";
    case RFAL_NFC_STATE_IDLE:               return "IDLE";
    case RFAL_NFC_STATE_START_DISCOVERY:    return "START_DISCOVERY";
    case RFAL_NFC_STATE_WAKEUP_MODE:        return "WAKEUP_MODE";
    case RFAL_NFC_STATE_POLL_TECHDETECT:    return "POLL_TECHDETECT";
    case RFAL_NFC_STATE_POLL_COLAVOIDANCE:  return "POLL_COLAVOIDANCE";
    case RFAL_NFC_STATE_POLL_SELECT:        return "POLL_SELECT";
    case RFAL_NFC_STATE_POLL_ACTIVATION:    return "POLL_ACTIVATION";
    case RFAL_NFC_STATE_LISTEN_TECHDETECT:  return "LISTEN_TECHDETECT";
    case RFAL_NFC_STATE_LISTEN_COLAVOIDANCE:return "LISTEN_COLAVOIDANCE";
    case RFAL_NFC_STATE_LISTEN_ACTIVATION:  return "LISTEN_ACTIVATION";
    case RFAL_NFC_STATE_LISTEN_SLEEP:       return "LISTEN_SLEEP";
    case RFAL_NFC_STATE_ACTIVATED:          return "ACTIVATED";
    case RFAL_NFC_STATE_DATAEXCHANGE:       return "DATAEXCHANGE";
    default: return "UNKNOWN";
  }
}

/// Deactivate and pump the worker until the stack returns to IDLE.
static void stopAndFlush(RfalNfcClass &nfc) {
  nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);
  for (int i = 0; i < 50; i++) {
    nfc.rfalNfcWorker();
    rfalNfcState st = nfc.rfalNfcGetState();
    if (st == RFAL_NFC_STATE_IDLE || st == RFAL_NFC_STATE_NOTINIT)
      break;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  Serial.printf("NFC: stopAndFlush — state = %s\r\n", nfcStateName(nfc.rfalNfcGetState()));
}

/// Ensure the RFAL stack is initialized and in IDLE state, ready for discover.
static bool ensureReady(RfalNfcClass &nfc) {
  rfalNfcState st = nfc.rfalNfcGetState();
  Serial.printf("NFC: ensureReady — current state = %s\r\n", nfcStateName(st));

  if (st == RFAL_NFC_STATE_NOTINIT) {
    Serial.println("NFC: stack not initialized, calling rfalNfcInitialize()...");
    ReturnCode err = nfc.rfalNfcInitialize();
    if (err != ERR_NONE) {
      Serial.printf("NFC: rfalNfcInitialize failed (err=%d)\r\n", err);
      return false;
    }
    st = nfc.rfalNfcGetState();
    Serial.printf("NFC: post-init state = %s\r\n", nfcStateName(st));
  }

  if (st != RFAL_NFC_STATE_IDLE) {
    // Try to get to IDLE
    nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);
    for (int i = 0; i < 50; i++) {
      nfc.rfalNfcWorker();
      st = nfc.rfalNfcGetState();
      if (st == RFAL_NFC_STATE_IDLE)
        break;
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    Serial.printf("NFC: state after flush = %s\r\n", nfcStateName(st));
  }

  return (st == RFAL_NFC_STATE_IDLE);
}

void NfcTask::run() {
  buildNdefFile();
  NfcMode currentMode = NfcMode::Off;

  NfcCommand cmd;
  for (;;) {
    // Block until we get a command
    if (!_nfcQueue.receive(cmd))
      continue;

    // Same mode pressed again = toggle off
    if (cmd.mode == currentMode && currentMode != NfcMode::Off) {
      Serial.printf("NFC: button toggle — stopping %s mode\r\n",
                     currentMode == NfcMode::Reader ? "reader" : "emulator");
      stopAndFlush(hw::nfcInstance());
      currentMode = NfcMode::Off;
      digitalWrite(badge::pins::NFC_LED, LOW);
      continue;
    }

    // Switching modes
    if (currentMode != NfcMode::Off) {
      Serial.printf("NFC: switching from %s to %s\r\n",
                     currentMode == NfcMode::Reader ? "reader" : "emulator",
                     cmd.mode == NfcMode::Reader ? "reader" : "emulator");
      stopAndFlush(hw::nfcInstance());
    }

    currentMode = cmd.mode;

    switch (cmd.mode) {
      case NfcMode::Reader:
        Serial.println("NFC: === ENTERING READER MODE ===");
        runReader();
        Serial.println("NFC: === EXITED READER MODE ===");
        break;
      case NfcMode::Emulator:
        Serial.println("NFC: === ENTERING EMULATOR MODE ===");
        runEmulator();
        Serial.println("NFC: === EXITED EMULATOR MODE ===");
        break;
      case NfcMode::Off:
        Serial.println("NFC: off");
        digitalWrite(badge::pins::NFC_LED, LOW);
        break;
    }

    // When runReader/runEmulator return, we're back to off
    currentMode = NfcMode::Off;
  }
}

void NfcTask::runReader() {
  RfalNfcClass &nfc = hw::nfcInstance();

  if (!ensureReady(nfc)) {
    Serial.println("NFC reader: failed to reach IDLE state");
    return;
  }

  rfalNfcDiscoverParam params;
  memset(&params, 0, sizeof(params));
  params.compMode = RFAL_COMPLIANCE_MODE_NFC;
  params.devLimit = 1;
  params.nfcfBR = RFAL_BR_212;
  params.ap2pBR = RFAL_BR_424;
  params.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
  params.GBLen = RFAL_NFCDEP_GB_MAX_LEN;
  params.totalDuration = 1000U;
  params.wakeupEnabled = false;
  params.wakeupConfigDefault = true;
  params.notifyCb = nullptr;

  ReturnCode err = nfc.rfalNfcDiscover(&params);
  if (err != ERR_NONE) {
    Serial.printf("NFC reader: discover failed (err=%d, state=%s)\r\n",
                  err, nfcStateName(nfc.rfalNfcGetState()));
    return;
  }

  Serial.println("NFC reader: polling for tags... (press A to stop, B for emulator)");

  for (;;) {
    // Check for new command (non-blocking)
    NfcCommand cmd;
    if (checkCommand(cmd)) {
      stopAndFlush(nfc);
      // Re-queue if it's a different mode
      if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Reader)
        _nfcQueue.send(cmd, Milliseconds(0));
      return;
    }

    nfc.rfalNfcWorker();
    rfalNfcState state = nfc.rfalNfcGetState();

    if (state == RFAL_NFC_STATE_ACTIVATED) {
      rfalNfcDevice *dev = nullptr;
      nfc.rfalNfcGetActiveDevice(&dev);

      if (dev) {
        digitalWrite(badge::pins::NFC_LED, HIGH);

        const char *typeName = "Unknown";
        if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA)
          typeName = "ISO14443A";
        else if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCB)
          typeName = "ISO14443B";
        else if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCV)
          typeName = "ISO15693";

        Serial.printf("NFC reader: %s tag detected!\r\n", typeName);
        Serial.print("  UID: ");
        for (uint8_t i = 0; i < dev->nfcidLen; i++)
          Serial.printf("%02X ", dev->nfcid[i]);
        Serial.printf(" (%d bytes)\r\n", dev->nfcidLen);

        // Flash green 3 times on RGB LEDs
        LedCommand ledCmd{};
        ledCmd.type = LedCommandType::ProgressFlash;
        ledCmd.pixelCount = 18;  // all LEDs
        ledCmd.r = 0;
        ledCmd.g = 255;
        ledCmd.b = 0;
        ledCmd.hold = false;
        _ledQueue.send(ledCmd, Milliseconds(0));

        digitalWrite(badge::pins::NFC_LED, LOW);
      }

      // Deactivate and exit reader mode after successful read
      stopAndFlush(nfc);
      Serial.println("NFC reader: read complete, exiting reader mode");
      return;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void NfcTask::runEmulator() {
  RfalNfcClass &nfc = hw::nfcInstance();

  if (!ensureReady(nfc)) {
    Serial.println("NFC emulator: failed to reach IDLE state");
    return;
  }

  rfalNfcDiscoverParam params;
  memset(&params, 0, sizeof(params));
  params.compMode = RFAL_COMPLIANCE_MODE_NFC;
  params.techs2Find = RFAL_NFC_LISTEN_TECH_A;
  params.totalDuration = 1000U;
  params.devLimit = 1;
  params.notifyCb = nullptr;

  // NFC-A listen mode: emulate Type 4 Tag
  params.lmConfigPA.nfcidLen = RFAL_LM_NFCID_LEN_04;
  params.lmConfigPA.nfcid[0] = 0x4E;  // 'N'
  params.lmConfigPA.nfcid[1] = 0x53;  // 'S'
  params.lmConfigPA.nfcid[2] = 0x45;  // 'E'
  params.lmConfigPA.nfcid[3] = 0x43;  // 'C'
  params.lmConfigPA.SENS_RES[0] = 0x04;
  params.lmConfigPA.SENS_RES[1] = 0x04;
  params.lmConfigPA.SEL_RES = 0x20;

  ReturnCode err = nfc.rfalNfcDiscover(&params);
  if (err != ERR_NONE) {
    Serial.printf("NFC emulator: discover failed (err=%d, state=%s)\r\n",
                  err, nfcStateName(nfc.rfalNfcGetState()));
    return;
  }

  Serial.println("NFC emulator: waiting for reader... (press B to stop, A for reader)");
  g_tagState = TAG_IDLE;

  for (;;) {
    // Check for new command (non-blocking)
    NfcCommand cmd;
    if (checkCommand(cmd)) {
      stopAndFlush(nfc);
      if (cmd.mode != NfcMode::Off && cmd.mode != NfcMode::Emulator)
        _nfcQueue.send(cmd, Milliseconds(0));
      return;
    }

    nfc.rfalNfcWorker();
    rfalNfcState state = nfc.rfalNfcGetState();

    if (state == RFAL_NFC_STATE_ACTIVATED) {
      digitalWrite(badge::pins::NFC_LED, HIGH);
      Serial.println("NFC emulator: === READER CONNECTED ===");
      g_tagState = TAG_IDLE;

      // Brief blue flash on RGB LEDs
      LedCommand ledCmd(LedCommandType::SolidBlue);
      _ledQueue.send(ledCmd, Milliseconds(0));

      // Data exchange loop
      uint8_t *rxData = nullptr;
      uint16_t *rxLen = nullptr;

      err = nfc.rfalNfcDataExchangeStart(nullptr, 0, &rxData, &rxLen, 0);
      if (err != ERR_NONE) {
        Serial.printf("NFC emulator: DataExchange start failed (err=%d)\r\n", err);
        nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
        digitalWrite(badge::pins::NFC_LED, LOW);
        continue;
      }

      // Process APDUs until link is lost or new command arrives
      bool linkActive = true;
      while (linkActive) {
        // Check for new command
        NfcCommand pendingCmd;
        if (checkCommand(pendingCmd)) {
          stopAndFlush(nfc);
          if (pendingCmd.mode != NfcMode::Off && pendingCmd.mode != NfcMode::Emulator)
            _nfcQueue.send(pendingCmd, Milliseconds(0));
          digitalWrite(badge::pins::NFC_LED, LOW);
          return;
        }

        nfc.rfalNfcWorker();
        err = nfc.rfalNfcDataExchangeGetStatus();

        if (err == ERR_BUSY) {
          vTaskDelay(pdMS_TO_TICKS(1));
          continue;
        }

        if (err == ERR_NONE && rxData && rxLen && *rxLen > 0) {
          Serial.printf("NFC emulator: RX APDU (%d bytes): ", *rxLen);
          for (uint16_t i = 0; i < *rxLen; i++)
            Serial.printf("%02X ", rxData[i]);
          Serial.println();

          uint16_t txLen = handleApdu(rxData, *rxLen, g_txBuf);

          Serial.printf("NFC emulator: TX APDU (%d bytes): ", txLen);
          for (uint16_t i = 0; i < txLen; i++)
            Serial.printf("%02X ", g_txBuf[i]);
          Serial.println();

          err = nfc.rfalNfcDataExchangeStart(g_txBuf, txLen, &rxData, &rxLen, 0);
          if (err != ERR_NONE) {
            Serial.printf("NFC emulator: TX failed (err=%d)\r\n", err);
            linkActive = false;
          }
        } else {
          Serial.printf("NFC emulator: link lost (err=%d)\r\n", err);
          linkActive = false;
        }
      }

      Serial.println("NFC emulator: reader disconnected");
      digitalWrite(badge::pins::NFC_LED, LOW);
      nfc.rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace core
