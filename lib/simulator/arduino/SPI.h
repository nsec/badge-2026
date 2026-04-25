#pragma once

// SPI stub for native simulator.

#include <cstdint>

#define HSPI      2
#define MSBFIRST  1
#define SPI_MODE0 0

struct SPISettings {
  SPISettings() = default;

  SPISettings(uint32_t /*freq*/, uint8_t /*bitOrder*/, uint8_t /*mode*/) {}
};

class SPIClass {
public:
  explicit SPIClass(uint8_t /*bus_num*/ = HSPI) {}

  void begin(int8_t /*sck*/ = -1, int8_t /*miso*/ = -1, int8_t /*mosi*/ = -1, int8_t /*ss*/ = -1) {}

  void beginTransaction(SPISettings /*settings*/) {}

  uint8_t transfer(uint8_t /*data*/) {
    return 0;
  }

  void endTransaction() {}

  void end() {}
};
