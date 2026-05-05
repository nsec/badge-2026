#include "hardware/dock.h"
#include "hardware/board_pins.h"
#include "hardware/hwid.h"
#include "hardware/rgb_led.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Queue handle for sending DockEvents from ISR to DockTask.
// Set via dockSetEventQueue() before tasks start.
static QueueHandle_t g_dockEventQueue = nullptr;

// Timestamp of last dock I2C activity (set from ISR, read from light task).
static volatile unsigned long g_lastDockActivityMs = 0;
static constexpr unsigned long DOCK_ACTIVE_TIMEOUT_MS = 5000;

// Custom response buffer for next I2C read, protected by critical section.
#define RESPONSE_BUF_MAX 64
static uint8_t g_responseBuf[RESPONSE_BUF_MAX];
static uint8_t g_responseLen = 0;
static volatile bool g_responseReady = false;
static portMUX_TYPE g_responseMux = portMUX_INITIALIZER_UNLOCKED;

// Challenge handler registry (struct defined in dock.h)
core::hw::ChallengeHandlerEntry g_challengeHandlers[core::hw::MAX_CHALLENGE_HANDLERS];
uint8_t g_challengeHandlerCount = 0;

namespace {

// The 12-char hex hardware ID, built once at init
char g_hwidHex[13] = {};

// I2C receive handler — called from Wire library context (ISR-like).
// Sends events to DockTask via FreeRTOS queue instead of using volatile flags.
void onReceive(int numBytes) {
  if (numBytes < 1)
    return;

  g_lastDockActivityMs = millis();

  uint8_t cmd = Wire.read();
  ets_printf("Dock ISR: onReceive cmd=0x%02X bytes=%d\n", cmd, numBytes);
  core::DockEvent evt;
  bool sendEvent = false;

  switch (static_cast<core::hw::DockCmd>(cmd)) {
    case core::hw::DockCmd::RequestHwid:
      // Nothing to do here — the response is sent in onRequest
      break;

    case core::hw::DockCmd::SetLedColor:
      if (numBytes >= 2) {
        evt.type = core::DockEventType::LedColor;
        evt.ledColor = static_cast<core::hw::DockLedColor>(Wire.read());
        sendEvent = true;
      }
      break;

    case core::hw::DockCmd::SendDockId:
      if (numBytes >= 2) {
        uint8_t dockId = Wire.read();
        if (dockId > 0) {
          evt.type = core::DockEventType::DockId;
          evt.dockId = dockId;
          sendEvent = true;
        }
      }
      break;

    case core::hw::DockCmd::ChallengeData:
      if (numBytes >= 2) {
        evt.type = core::DockEventType::ChallengeData;
        evt.challenge.subOpcode = Wire.read();
        evt.challenge.dataLen = 0;
        while (Wire.available() && evt.challenge.dataLen < core::hw::CHALLENGE_DATA_MAX) {
          evt.challenge.data[evt.challenge.dataLen++] = Wire.read();
        }
        sendEvent = true;
      }
      break;

    default:
      break;
  }

  // Drain any remaining bytes
  while (Wire.available())
    Wire.read();

  if (sendEvent && g_dockEventQueue) {
    BaseType_t woken = pdFALSE;
    BaseType_t sent = xQueueSendFromISR(g_dockEventQueue, &evt, &woken);
    if (sent != pdTRUE) {
      ets_printf("Dock ISR: queue full, event dropped (type=%d)\n", (int)evt.type);
    }
    portYIELD_FROM_ISR(woken);
  } else if (sendEvent) {
    ets_printf("Dock ISR: no queue handle!\n");
  }
}

// I2C request handler — called when dock reads from badge
void onRequest() {
  g_lastDockActivityMs = millis();
  taskENTER_CRITICAL_ISR(&g_responseMux);
  if (g_responseReady && g_responseLen > 0) {
    Wire.write(g_responseBuf, g_responseLen);
    g_responseReady = false;
    g_responseLen = 0;
  } else {
    Wire.write(reinterpret_cast<const uint8_t *>(g_hwidHex), 12);
  }
  taskEXIT_CRITICAL_ISR(&g_responseMux);
}

}  // namespace

namespace core {
namespace hw {

void dockSetEventQueue(void *queueHandle) {
  g_dockEventQueue = static_cast<QueueHandle_t>(queueHandle);
}

bool dockIsActive() {
  unsigned long last = g_lastDockActivityMs;
  if (last == 0)
    return false;
  return (millis() - last) < DOCK_ACTIVE_TIMEOUT_MS;
}

void dockInit() {
  // Build the hardware ID hex string
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);
  snprintf(g_hwidHex, sizeof(g_hwidHex), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  bool ok = Wire.begin(DOCK_I2C_ADDR, badge::pins::I2C_SDA, badge::pins::I2C_SCL, 0);
  if (!ok) {
    Serial.println("Dock: I2C slave init FAILED");
    return;
  }
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  Serial.printf("I2C initialized\r\n");
}

void dockReinitSlave() {
  Wire.end();
  Wire.begin(DOCK_I2C_ADDR, badge::pins::I2C_SDA, badge::pins::I2C_SCL, 0);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
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
  taskENTER_CRITICAL(&g_responseMux);
  memcpy(g_responseBuf, data, len);
  g_responseLen = len;
  g_responseReady = true;
  taskEXIT_CRITICAL(&g_responseMux);
}

}  // namespace hw
}  // namespace core
