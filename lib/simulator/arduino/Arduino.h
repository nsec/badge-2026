#pragma once

// Arduino API stubs for native/desktop simulator build
// IMPORTANT: We do NOT define min/max macros to avoid conflicts with std::min/std::max
// Arduino code should use std::min/std::max instead.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

// Arduino types
typedef bool boolean;
typedef uint8_t byte;
using String = std::string;

// Pin modes
#define INPUT          0
#define OUTPUT         1
#define INPUT_PULLUP   2
#define INPUT_PULLDOWN 3

// Pin states
#define LOW  0
#define HIGH 1

// Use std::min/std::max - no macros defined to avoid conflicts
// If your code needs min/max macros, use std::min/std::max directly

// abs macro removed — conflicts with C++ stdlib internals (riemann_zeta.tcc).
// Arduino code should use the C++ std::abs() or a local inline instead.
#ifndef constrain
  #define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

// Bit manipulation
#define bitRead(value, bit)            (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)             ((value) |= (1UL << (bit)))
#define bitClear(value, bit)           ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#define bit(b)                         (1UL << (b))

// Number bases for print
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// Timing functions
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// GPIO functions
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

// Analog (stub)
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);

// Character functions (pass through to ctype)
inline boolean isAlpha(int c) {
  return std::isalpha(c);
}

inline boolean isAlphaNumeric(int c) {
  return std::isalnum(c);
}

inline boolean isAscii(int c) {
  return (c >= 0 && c <= 127);
}

inline boolean isControl(int c) {
  return std::iscntrl(c);
}

inline boolean isDigit(int c) {
  return std::isdigit(c);
}

inline boolean isGraph(int c) {
  return std::isgraph(c);
}

inline boolean isHexadecimalDigit(int c) {
  return std::isxdigit(c);
}

inline boolean isLowerCase(int c) {
  return std::islower(c);
}

inline boolean isPrintable(int c) {
  return std::isprint(c);
}

inline boolean isPunct(int c) {
  return std::ispunct(c);
}

inline boolean isSpace(int c) {
  return std::isspace(c);
}

inline boolean isUpperCase(int c) {
  return std::isupper(c);
}

inline boolean isWhitespace(int c) {
  return std::isspace(c);
}

// Include Print and Stream
#include "Print.h"
#include "Stream.h"

// ESP class stub
class ESPClass {
public:
  void restart();
  uint32_t getFreeHeap();
  uint32_t getChipId();
  const char *getSdkVersion();
};

extern ESPClass ESP;

// Serial object declaration (defined in Stream.cpp)
extern class HardwareSerial Serial;
