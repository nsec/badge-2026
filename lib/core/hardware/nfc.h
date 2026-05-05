#pragma once

#ifndef NATIVE_BUILD
  #include <SPI.h>
  #include <rfal_rf.h>
  #include <rfal_nfc.h>
  #include <rfal_rfst25r3916.h>
#endif

namespace core {
namespace hw {

/// Initialize the NFC SPI bus and RFAL stack.  Returns true on success.
bool nfcInit();

#ifndef NATIVE_BUILD
/// Get the RFAL NFC class instance (high-level, for reader discovery).
RfalNfcClass &nfcInstance();

/// Get the RFAL hardware driver (low-level, for emulator listen mode).
RfalRfST25R3916Class &nfcHardware();

/// Get the dedicated NFC SPI bus.
SPIClass &nfcSPI();
#endif

}  // namespace hw
}  // namespace core
