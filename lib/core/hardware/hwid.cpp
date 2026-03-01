#include "hwid.h"

#include <Arduino.h>
#include "esp_mac.h"

namespace core {
namespace hw {

void getHwidMac(uint8_t out[MAC_LEN]) {
  esp_efuse_mac_get_default(out);
}

uint8_t getHwidObfuscationByte() {
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);

  // Mix all 6 MAC bytes with multiply-XOR to spread entropy.
  // Much harder to reverse than a simple XOR fold.
  uint8_t h = 0x55;  // seed
  for (int i = 0; i < MAC_LEN; i++) {
    h ^= mac[i];
    h = static_cast<uint8_t>((h * 31) + mac[i]);
  }
  return h;
}

void printHardwareId(Stream &io) {
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);

  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  io.print("Hardware ID: ");
  io.println(buf);
}

}  // namespace hw
}  // namespace core
