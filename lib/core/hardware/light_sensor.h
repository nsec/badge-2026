#pragma once
/**
 * light_sensor.h — VEML6040 RGBW light sensor driver.
 *
 * Reads colour channels and estimates lux + CCT.
 * Shares the I2C bus (Wire) with the dock subsystem.
 */

#include <cstdint>

namespace core {
namespace hw {

/// Initialize the VEML6040 sensor (must be called after Wire.begin or dockInit).
bool lightSensorInit();

/// Returns true if lightSensorInit() succeeded.
bool lightSensorReady();

/// Raw channel readings from a single poll.
struct LightReading {
  uint16_t r, g, b, w;
  float lux;
  float cct;
};

/// Read all four channels + compute lux and CCT. Returns false on I2C error.
bool lightSensorRead(LightReading &out);

/// Lux threshold above which the badge is considered "in light".
/// ~200 lux ≈ well-lit conference floor. Dim corridors (~100) don't count.
static constexpr float LIGHT_LUX_THRESHOLD = 200.0f;

}  // namespace hw
}  // namespace core
