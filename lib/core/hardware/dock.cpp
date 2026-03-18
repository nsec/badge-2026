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

namespace {

// The 12-char hex hardware ID, built once at init
char g_hwidHex[13] = {};

// I2C receive handler — called when dock sends a command
void onReceive(int numBytes) {
  if (numBytes < 1)
    return;

  Serial.printf("Dock I2C: received %d bytes\r\n", numBytes);

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

    default:
      break;
  }

  // Drain any remaining bytes
  while (Wire.available())
    Wire.read();
}

// I2C request handler — called when dock reads from badge
void onRequest() {
  Serial.printf("Dock I2C: request received, sending HWID: %s\r\n", g_hwidHex);
  Wire.write(reinterpret_cast<const uint8_t *>(g_hwidHex), 12);
}

}  // namespace

namespace core {
namespace hw {

void dockInit() {
  // Build the hardware ID hex string
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);
  snprintf(g_hwidHex, sizeof(g_hwidHex), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

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

  //Serial.printf("I2C follower ready on 0x%02X, SDA=%d, SCL=%d (HWID: %s)\r\n",
  //              DOCK_I2C_ADDR, badge::pins::I2C_SDA, badge::pins::I2C_SCL, g_hwidHex);
  Serial.printf("I2C initialized\r\n");
}

}  // namespace hw
}  // namespace core
