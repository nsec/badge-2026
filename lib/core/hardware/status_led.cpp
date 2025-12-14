#include "status_led.h"

#include "board_pins.h"

namespace core {
namespace hw {

void statusLedInit() {
  pinMode(badge::pins::LED_STATUS, OUTPUT);
  statusLedSet(false);
}

void statusLedSet(bool on) {
  digitalWrite(badge::pins::LED_STATUS, on ? HIGH : LOW);
}

} // namespace hw
} // namespace core
