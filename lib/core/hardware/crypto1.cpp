#include "hardware/crypto1.h"

// Adapted from the crapto1 library by blapost

#define LF_POLY_ODD  (0x29CE5C)
#define LF_POLY_EVEN (0x870804)

#define BIT(x, n)   (((x) >> (n)) & 1)
#define BEBIT(x, n) BIT(x, (n) ^ 24)

static uint8_t parity32(uint32_t x) {
  x ^= x >> 16;
  x ^= x >> 8;
  x ^= x >> 4;
  return (0x6996 >> (x & 0xf)) & 1;
}

static uint8_t filter(uint32_t const x) {
  uint32_t f;
  f = 0xf22c0 >> (x & 0xf) & 16;
  f |= 0x6c9c0 >> (x >> 4 & 0xf) & 8;
  f |= 0x3c8b0 >> (x >> 8 & 0xf) & 4;
  f |= 0x1e458 >> (x >> 12 & 0xf) & 2;
  f |= 0x0d938 >> (x >> 16 & 0xf) & 1;
  return BIT(0xEC57E80A, f);
}

void crypto1_init(Crypto1 *c, const uint8_t key[6]) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) {
    k = (k << 8) | key[i];
  }
  c->odd = c->even = 0;
  for (int i = 47; i > 0; i -= 2) {
    c->odd = c->odd << 1 | BIT(k, (i - 1) ^ 7);
    c->even = c->even << 1 | BIT(k, i ^ 7);
  }
}

uint8_t crypto1_bit(Crypto1 *c, uint8_t input, bool isEncrypted) {
  uint8_t ret = filter(c->odd);
  uint32_t feedin;
  feedin = ret & (uint32_t)(isEncrypted ? 1 : 0);
  feedin ^= (uint32_t)(input & 1);
  feedin ^= LF_POLY_ODD & c->odd;
  feedin ^= LF_POLY_EVEN & c->even;
  c->even = c->even << 1 | parity32(feedin);
  uint32_t tmp = c->odd;
  c->odd = c->even;
  c->even = tmp;
  return ret;
}

uint8_t crypto1_byte(Crypto1 *c, uint8_t input, bool isEncrypted) {
  uint8_t ret = 0;
  for (int i = 0; i < 8; i++) {
    ret |= crypto1_bit(c, BIT(input, i), isEncrypted) << i;
  }
  return ret;
}

uint32_t crypto1_word(Crypto1 *c, uint32_t input, bool isEncrypted) {
  uint32_t ret = 0;
  for (int i = 0; i < 32; i++) {
    ret |= (uint32_t)crypto1_bit(c, BEBIT(input, i), isEncrypted) << (i ^ 24);
  }
  return ret;
}

uint8_t crypto1_peek(Crypto1 *c) {
  return filter(c->odd);
}

uint32_t prng_successor(uint32_t x, uint32_t n) {
  x = ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) | ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
  while (n--) {
    x = (x >> 1) | ((((x >> 16) ^ (x >> 18) ^ (x >> 19) ^ (x >> 21)) & 1) << 31);
  }
  x = ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) | ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
  return x;
}
