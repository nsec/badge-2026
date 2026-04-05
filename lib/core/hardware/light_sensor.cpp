#include "hardware/light_sensor.h"
#include "hardware/board_pins.h"

#include <Arduino.h>
#include <Wire.h>

// Use Wire1 (second I2C peripheral) for the light sensor.
// Wire (first peripheral) is used by the dock subsystem in slave mode
// and cannot do master transactions simultaneously on ESP32-S3.
// Both share the same physical SDA/SCL pins — ESP32-S3 supports this
// via its I2C peripheral multiplexing.
static TwoWire &LightWire = Wire1;
static bool g_wireInitDone = false;

// VEML6040 I2C address and registers
#define VEML6040_ADDR 0x10
#define VEML6040_CONF 0x00
#define VEML6040_R    0x08
#define VEML6040_G    0x09
#define VEML6040_B    0x0A
#define VEML6040_W    0x0B

// Config: 160ms integration, auto mode, enabled
#define VEML6040_IT_160MS  (0x02 << 4)
#define VEML6040_AF_AUTO   0x00
#define VEML6040_SD_ENABLE 0x00

static bool g_lightReady = false;

static void writeReg(uint8_t reg, uint16_t value) {
  LightWire.beginTransmission(VEML6040_ADDR);
  LightWire.write(reg);
  LightWire.write(value & 0xFF);
  LightWire.write(value >> 8);
  LightWire.endTransmission();
}

static bool readReg(uint8_t reg, uint16_t &out) {
  LightWire.beginTransmission(VEML6040_ADDR);
  LightWire.write(reg);
  if (LightWire.endTransmission(false) != 0)
    return false;
  if (LightWire.requestFrom((uint8_t)VEML6040_ADDR, (uint8_t)2) != 2)
    return false;
  out = LightWire.read();
  out |= static_cast<uint16_t>(LightWire.read()) << 8;
  return true;
}

namespace core {
namespace hw {

bool lightSensorInit() {
  // Initialize Wire1 as I2C master on the same SDA/SCL pins
  if (!g_wireInitDone) {
    LightWire.begin(badge::pins::I2C_SDA, badge::pins::I2C_SCL, 100000);
    g_wireInitDone = true;
  }

  // Configure: 160ms integration, auto mode, sensor enabled
  writeReg(VEML6040_CONF, VEML6040_IT_160MS | VEML6040_AF_AUTO | VEML6040_SD_ENABLE);
  delay(200);

  // Verify sensor is responding by reading green channel
  uint16_t g = 0;
  if (!readReg(VEML6040_G, g))
    return false;

  g_lightReady = true;
  return true;
}

bool lightSensorRead(LightReading &out) {
  if (!g_lightReady)
    return false;

  if (!readReg(VEML6040_R, out.r))
    return false;
  if (!readReg(VEML6040_G, out.g))
    return false;
  if (!readReg(VEML6040_B, out.b))
    return false;
  if (!readReg(VEML6040_W, out.w))
    return false;

  // Lux from green channel (0.25168 sensitivity for 160ms integration)
  out.lux = out.g * 0.25168f;

  // Estimate CCT from spectral ratio (Vishay VEML6040 app note)
  if (out.g > 0) {
    float ccti = (static_cast<float>(out.r) - static_cast<float>(out.b)) / static_cast<float>(out.g) + 0.5f;
    out.cct = (ccti > 0) ? 4278.6f * powf(ccti, -1.2455f) : 10000.f;
  } else {
    out.cct = 0;
  }

  return true;
}

}  // namespace hw
}  // namespace core
