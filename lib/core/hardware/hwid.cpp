#include "hwid.h"

#include <Arduino.h>
#include "esp_mac.h"

namespace core {
namespace hw {

String getHardwareId() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void printHardwareId(Stream &io) {
  io.print("Hardware ID: ");
  io.println(getHardwareId());
}

}  // namespace hw
}  // namespace core
