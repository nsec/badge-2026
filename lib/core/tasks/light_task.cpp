#include "tasks/light_task.h"

#include <Arduino.h>

#include "hardware/light_sensor.h"
#include "hardware/serial_mutex.h"
#include "storage/nvs_social.h"

namespace core {

// Last reading — accessible via lightLastReading() for the CLI.
static hw::LightReading g_lastReading = {};
static bool g_lastReadingValid = false;

bool lightLastReading(hw::LightReading &out) {
  if (!g_lastReadingValid)
    return false;
  out = g_lastReading;
  return true;
}

void LightTask::run() {
  // Sensor is initialized in main.cpp setup(); if it failed, stop this task.
  if (!hw::lightSensorReady()) {
    vTaskDelete(nullptr);
    return;
  }

  // Fractional accumulator for smooth NVS updates.
  float lightAccum = static_cast<float>(storage::socialRead(storage::SocialKey::Light));

  static constexpr uint32_t POLL_MS = 5000;        // 5 second sensor poll
  static constexpr uint32_t NVS_WRITE_POLLS = 6;   // write NVS every 6 polls (30s)
  static constexpr uint32_t MAX_HOLD_POLLS = 360;  // hold at 255 for 360 polls (30 min) before decay
  uint32_t pollsSinceWrite = 0;
  uint32_t maxHoldCounter = 0;  // counts down while holding at 255

  // If we loaded 255 from NVS, start with the hold active
  if (lightAccum >= 255.f)
    maxHoldCounter = MAX_HOLD_POLLS;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(POLL_MS));

    hw::LightReading reading;
    if (!hw::lightSensorRead(reading))
      continue;

    g_lastReading = reading;
    g_lastReadingValid = true;

    // Scaled increment/decrement based on distance from threshold.
    if (reading.lux >= hw::LIGHT_LUX_THRESHOLD) {
      float ratio = reading.lux / hw::LIGHT_LUX_THRESHOLD;
      if (ratio > 5.f)
        ratio = 5.f;
      lightAccum += 0.2f * ratio;

      // If we hit max, start/refresh the hold timer
      if (lightAccum >= 255.f) {
        lightAccum = 255.f;
        maxHoldCounter = MAX_HOLD_POLLS;
      }
    } else {
      // Below threshold — only decay if hold timer has expired
      if (maxHoldCounter > 0) {
        maxHoldCounter--;
      } else {
        float darkRatio = 1.f - reading.lux / hw::LIGHT_LUX_THRESHOLD;
        if (darkRatio > 1.f)
          darkRatio = 1.f;
        lightAccum -= 0.02f + 0.04f * darkRatio;
      }
    }

    // Clamp to [0, 255]
    if (lightAccum > 255.f)
      lightAccum = 255.f;
    if (lightAccum < 0.f)
      lightAccum = 0.f;

    // Write to NVS periodically (every ~30s) to reduce flash wear
    pollsSinceWrite++;
    if (pollsSinceWrite >= NVS_WRITE_POLLS) {
      pollsSinceWrite = 0;
      uint8_t nvsVal = static_cast<uint8_t>(lightAccum);
      uint8_t current = storage::socialRead(storage::SocialKey::Light);
      if (nvsVal != current) {
        storage::socialWrite(storage::SocialKey::Light, nvsVal);
      }
    }
  }
}

}  // namespace core
