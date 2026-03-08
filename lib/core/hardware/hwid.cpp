#include "hwid.h"

#include <Arduino.h>
#include "esp_mac.h"

namespace core {
namespace hw {

void getHwidMac(uint8_t out[MAC_LEN]) {
  esp_efuse_mac_get_default(out);
}

void printHardwareId(Stream &io) {
  uint8_t mac[MAC_LEN];
  getHwidMac(mac);

  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  io.print("Hardware ID: ");
  io.println(buf);
}

}  // namespace hw
}  // namespace core
