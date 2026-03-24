#include "hardware/serial_mutex.h"

static core::hw::LineBufferedStream *g_safeSerial = nullptr;

namespace core {
namespace hw {

void setSafeSerial(LineBufferedStream *lbs) {
  g_safeSerial = lbs;
}

LineBufferedStream &safeSerial() {
  return *g_safeSerial;
}

}  // namespace hw
}  // namespace core
