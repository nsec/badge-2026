#include "hardware/dock.h"
#include "hardware/board_pins.h"
#include "hardware/hwid.h"
#include "hardware/rgb_led.h"

#include <Arduino.h>
#include <Wire.h>

// Shared volatile state — accessed by dock_task.cpp via extern
volatile core::hw::DockLedColor g_pendingLedColor = core::hw::DockLedColor::Off;
volatile bool g_ledColorPending = false;
volatile uint8_t g_pendingDockId = 0;
volatile bool g_dockIdPending = false;

// Custom response buffer for next I2C read
#define RESPONSE_BUF_MAX 64
static uint8_t g_responseBuf[RESPONSE_BUF_MAX];
static uint8_t g_responseLen = 0;
static volatile bool g_responseReady = false;

// Challenge data buffer — filled in ISR, consumed by dock task
#define CHALLENGE_DATA_MAX 32
volatile uint8_t g_challengeSubOpcode = 0;
volatile uint8_t g_challengeData[CHALLENGE_DATA_MAX];
volatile uint8_t g_challengeDataLen = 0;
volatile bool g_challengeDataPending = false;

// Challenge handler registry
#define MAX_CHALLENGE_HANDLERS 8

struct ChallengeHandlerEntry {
  uint8_t subOpcode;
  core::hw::DockChallengeHandler handler;
};

ChallengeHandlerEntry g_challengeHandlers[MAX_CHALLENGE_HANDLERS];
uint8_t g_challengeHandlerCount = 0;

namespace {

// The 12-char hex hardware ID, built once at init
char g_hwidHex[13] = {};

// I2C receive handler — called when dock sends a command
void onReceive(int numBytes) {
  if (numBytes < 1)
    return;

  // NOTE: Do NOT call Serial from this callback — it runs in
  // Wire library context (ISR-like) and will corrupt USB-CDC output.

  uint8_t cmd = Wire.read();

  switch (static_cast<core::hw::DockCmd>(cmd)) {
    case core::hw::DockCmd::RequestHwid:
      // Nothing to do here — the response is sent in onRequest
      break;

    case core::hw::DockCmd::SetLedColor:
      if (numBytes >= 2) {
        uint8_t color = Wire.read();
        g_pendingLedColor = static_cast<core::hw::DockLedColor>(color);
        g_ledColorPending = true;
      }
      break;

    case core::hw::DockCmd::SendDockId:
      if (numBytes >= 2) {
        uint8_t dockId = Wire.read();
        if (dockId > 0) {
          g_pendingDockId = dockId;
          g_dockIdPending = true;
        }
      }
      break;

    case core::hw::DockCmd::ChallengeData:
      if (numBytes >= 2) {
        g_challengeSubOpcode = Wire.read();
        g_challengeDataLen = 0;
        while (Wire.available() && g_challengeDataLen < CHALLENGE_DATA_MAX) {
          g_challengeData[g_challengeDataLen++] = Wire.read();
        }
        g_challengeDataPending = true;
      }
      break;

    default:
      break;
  }

  // Drain any remaining bytes
  while (Wire.available())
    Wire.read();
}

// I2C request handler — called when dock reads from badge
void onRequest() {
  if (g_responseReady && g_responseLen > 0) {
    // Send custom response (e.g. quantum challenge data)
    Wire.write(g_responseBuf, g_responseLen);
    g_responseReady = false;
    g_responseLen = 0;
  } else {
    // Default: send hardware ID
    Wire.write(reinterpret_cast<const uint8_t *>(g_hwidHex), 12);
  }
}

}  // namespace

namespace core {
namespace hw {

void dockInit() {
  // Build the hardware ID hex string
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);
  snprintf(g_hwidHex, sizeof(g_hwidHex), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Initialize I2C slave on the badge
  // For ESP32-S3 Arduino: Wire.begin(addr, sda, scl, freq)
  // Slave mode: frequency is ignored but SDA/SCL must be set
  bool ok = Wire.begin(DOCK_I2C_ADDR, badge::pins::I2C_SDA, badge::pins::I2C_SCL, 0);
  if (!ok) {
    Serial.println("Dock: I2C slave init FAILED");
    return;
  }
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  // Serial.printf("I2C follower ready on 0x%02X, SDA=%d, SCL=%d (HWID: %s)\r\n",
  //               DOCK_I2C_ADDR, badge::pins::I2C_SDA, badge::pins::I2C_SCL, g_hwidHex);
  Serial.printf("I2C initialized\r\n");
}

void dockRegisterChallengeHandler(uint8_t subOpcode, DockChallengeHandler handler) {
  if (g_challengeHandlerCount < MAX_CHALLENGE_HANDLERS) {
    g_challengeHandlers[g_challengeHandlerCount].subOpcode = subOpcode;
    g_challengeHandlers[g_challengeHandlerCount].handler = handler;
    g_challengeHandlerCount++;
  }
}

void dockSetResponseBuffer(const uint8_t *data, uint8_t len) {
  if (len > RESPONSE_BUF_MAX)
    len = RESPONSE_BUF_MAX;
  memcpy(g_responseBuf, data, len);
  g_responseLen = len;
  g_responseReady = true;
}

}  // namespace hw
}  // namespace core
