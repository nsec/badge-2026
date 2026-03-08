#pragma once

#include <cstdint>

// Crypto1 stream cipher for Mifare Classic
// Based on the crapto1 implementation (split odd/even LFSR representation)

struct Crypto1 {
  uint32_t odd;   // LFSR bits at even positions (0,2,4,...,46) - 24 bits
  uint32_t even;  // LFSR bits at odd positions (1,3,5,...,47) - 24 bits
};

void crypto1_init(Crypto1 *c, const uint8_t key[6]);
uint8_t crypto1_bit(Crypto1 *c, uint8_t input, bool isEncrypted);
uint8_t crypto1_byte(Crypto1 *c, uint8_t input, bool isEncrypted);
uint32_t crypto1_word(Crypto1 *c, uint32_t input, bool isEncrypted);
uint8_t crypto1_peek(Crypto1 *c);
uint32_t prng_successor(uint32_t x, uint32_t n);
