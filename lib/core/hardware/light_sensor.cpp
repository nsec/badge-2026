#include "hardware/light_sensor.h"
#include "hardware/board_pins.h"
#include "hardware/dock.h"

#include <Arduino.h>
#include <Wire.h>

// Use Wire1 (second I2C peripheral) as a temporary I2C master for the sensor.
// Wire (first peripheral) is used by the dock subsystem in slave mode.
// Both share the same physical SDA/SCL pins, so only one can own the GPIO
// output mux at a time. We bring up Wire1 for each transaction, then tear it
// down and re-init the dock slave so dock I2C stays functional.
static TwoWire &LightWire = Wire1;

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

/// Temporarily claim the I2C bus for master transactions.
static void busAcquire() {
  LightWire.begin(badge::pins::I2C_SDA, badge::pins::I2C_SCL, 100000);
}

/// Release the I2C bus and restore the dock's slave ownership.
static void busRelease() {
  LightWire.end();
  core::hw::dockReinitSlave();
}

namespace core {
namespace hw {

bool lightSensorInit() {
  busAcquire();

  // Configure: 160ms integration, auto mode, sensor enabled
  writeReg(VEML6040_CONF, VEML6040_IT_160MS | VEML6040_AF_AUTO | VEML6040_SD_ENABLE);
  delay(200);

  // Verify sensor is responding by reading green channel
  uint16_t g = 0;
  bool ok = readReg(VEML6040_G, g);

  busRelease();

  if (!ok)
    return false;

  g_lightReady = true;
  return true;
}

bool lightSensorReady() {
  return g_lightReady;
}

bool lightSensorRead(LightReading &out) {
  if (!g_lightReady)
    return false;

  busAcquire();

  bool ok = readReg(VEML6040_R, out.r) && readReg(VEML6040_G, out.g) && readReg(VEML6040_B, out.b) &&
            readReg(VEML6040_W, out.w);

  // Re-configure sensor each poll — the config register may have been lost
  // when we tore down Wire1 on the previous cycle.
  writeReg(VEML6040_CONF, VEML6040_IT_160MS | VEML6040_AF_AUTO | VEML6040_SD_ENABLE);

  busRelease();

  if (!ok)
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
