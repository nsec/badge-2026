#include "Arduino.h"

#include <chrono>
#include <thread>
#include <unordered_map>
#include <cstdlib>

// Time tracking
static auto g_start_time = std::chrono::steady_clock::now();

unsigned long millis() {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - g_start_time).count();
}

unsigned long micros() {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now - g_start_time).count();
}

void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void delayMicroseconds(unsigned int us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// GPIO state tracking
static std::unordered_map<uint8_t, uint8_t> g_pin_modes;
static std::unordered_map<uint8_t, uint8_t> g_pin_states;

void pinMode(uint8_t pin, uint8_t mode) {
  g_pin_modes[pin] = mode;
  if (mode == OUTPUT) {
    g_pin_states[pin] = LOW;
  }
}

void digitalWrite(uint8_t pin, uint8_t val) {
  g_pin_states[pin] = val ? HIGH : LOW;
}

int digitalRead(uint8_t pin) {
  auto it = g_pin_states.find(pin);
  if (it != g_pin_states.end()) {
    return it->second;
  }
  // Default: LOW for undefined pins
  return LOW;
}

int analogRead(uint8_t pin) {
  (void)pin;
  return 0;  // Stub
}

void analogWrite(uint8_t pin, int val) {
  (void)pin;
  (void)val;
  // Stub
}

// ESP class implementation
ESPClass ESP;

void ESPClass::restart() {
  std::exit(0);
}

uint32_t ESPClass::getFreeHeap() {
  return 256 * 1024;  // Fake 256KB
}

uint32_t ESPClass::getChipId() {
  return 0x12345678;  // Fake ID
}

const char *ESPClass::getSdkVersion() {
  return "simulator-1.0.0";
}
