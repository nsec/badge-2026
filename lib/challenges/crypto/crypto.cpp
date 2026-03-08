#include "crypto.h"
#include <Arduino.h>
#include <string>
#include <../core/tasks/cli.h>

namespace challenges {
namespace crypto {

void init() {
  Serial.println("  - Crypto challenge module loaded");
  core::cli::registerCommand("crypto", "crypto challenge info", [](Stream &stream, const std::string &args) {
    handleCryptoCommand(stream);
  });
}

void handleCryptoCommand(Stream &stream) {
  stream.println("=== Crypto Challenge: The Cipher ===");
  stream.println();
  stream.println("You've intercepted an encrypted message:");
  stream.println();
  stream.println("  Gur cnffjbeq vf: ONFR64");
  stream.println();
  stream.println("Hints:");
  stream.println("  - This is a classic substitution cipher");
  stream.println("  - Try rotating the alphabet");
  stream.println("  - ROT13 might help...");
  stream.println();
  stream.println("Once decoded, submit the flag with:");
  stream.println("  flag <your_answer>");
}

}  // namespace crypto
}  // namespace challenges
