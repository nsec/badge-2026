#include "tasks/heartbeat.h"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <badge_config.h>
#include "hardware/status_led.h"

namespace {

TimerHandle_t g_heartbeatTimer = nullptr;
bool g_ledState = false;

void heartbeatCallback(TimerHandle_t) {
  g_ledState = !g_ledState;
  core::hw::statusLedSet(g_ledState);
}

}  // namespace

namespace core {

void heartbeatStart() {
  configASSERT(!g_heartbeatTimer);

  g_heartbeatTimer = xTimerCreate(
      "heartbeat", pdMS_TO_TICKS(badge::config::heartbeat::blink_interval.count()), pdTRUE,
      nullptr, heartbeatCallback);
  configASSERT(g_heartbeatTimer);

  BaseType_t ok = xTimerStart(g_heartbeatTimer, 0);
  configASSERT(ok == pdPASS);
}

}  // namespace core
