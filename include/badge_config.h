#pragma once

// Project-wide compile-time settings.

#ifndef NSEC_BADGE_YEAR
#define NSEC_BADGE_YEAR 2026
#endif

#include <chrono>

#include <freertos/FreeRTOS.h>

namespace badge::config {

namespace heartbeat {
inline constexpr std::chrono::milliseconds blink_interval{500};
}  // namespace heartbeat

namespace buttons {
inline constexpr std::chrono::milliseconds debounce_delay{15};
inline constexpr std::chrono::milliseconds long_press_time{800};
inline constexpr std::chrono::milliseconds hold_poll_timeout{100};
}  // namespace buttons

namespace queues {
inline constexpr UBaseType_t controller_depth = 16;
inline constexpr UBaseType_t led_depth = 8;
inline constexpr UBaseType_t cli_depth = 4;
}  // namespace queues

namespace tasks {
inline constexpr UBaseType_t priority_cli = 1;
inline constexpr UBaseType_t priority_controller = 2;
inline constexpr UBaseType_t priority_led = 3;
inline constexpr UBaseType_t priority_button = 5;
}  // namespace tasks

}  // namespace badge::config
