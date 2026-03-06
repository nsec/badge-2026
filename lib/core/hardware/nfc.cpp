#include "hardware/nfc.h"
#include "hardware/board_pins.h"

#include <Arduino.h>

namespace {

SPIClass g_nfcSPI(HSPI);
RfalRfST25R3916Class g_hardware(&g_nfcSPI, badge::pins::NFC_CS, badge::pins::NFC_INT);
RfalNfcClass g_nfc(&g_hardware);
bool g_initialized = false;

}  // namespace

namespace core {
namespace hw {

bool nfcInit() {
  if (g_initialized)
    return true;

  pinMode(badge::pins::NFC_LED, OUTPUT);
  digitalWrite(badge::pins::NFC_LED, LOW);
  pinMode(badge::pins::NFC_CS, OUTPUT);
  digitalWrite(badge::pins::NFC_CS, HIGH);

  g_nfcSPI.begin(badge::pins::NFC_SCK, badge::pins::NFC_MISO, badge::pins::NFC_MOSI);
  delay(100);

  ReturnCode err = g_nfc.rfalNfcInitialize();
  if (err != ERR_NONE) {
    Serial.printf("NFC: init failed (err=%d)\r\n", err);
    return false;
  }

  g_initialized = true;
  Serial.println("NFC: ST25R3916 initialized");
  return true;
}

RfalNfcClass &nfcInstance() {
  return g_nfc;
}

SPIClass &nfcSPI() {
  return g_nfcSPI;
}

}  // namespace hw
}  // namespace core
