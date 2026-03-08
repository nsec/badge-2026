#pragma once

#include <Stream.h>

namespace challenges {
namespace crypto {

/**
 * Initialize the crypto challenge module
 */
void init();

/**
 * Handle the 'crypto' command
 */
void handleCryptoCommand(Stream &stream);

}  // namespace crypto
}  // namespace challenges
