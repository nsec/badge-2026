#pragma once

#include <SPI.h>
#include <rfal_rf.h>
#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>

namespace core {
namespace hw {

/// Initialize the NFC SPI bus and RFAL stack.  Returns true on success.
bool nfcInit();

/// Get the RFAL NFC class instance (for use by the NFC task).
RfalNfcClass &nfcInstance();

/// Get the dedicated NFC SPI bus.
SPIClass &nfcSPI();

}  // namespace hw
}  // namespace core
