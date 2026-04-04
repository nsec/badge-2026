#pragma once

// Project-wide compile-time settings.

#ifndef NSEC_BADGE_YEAR
#define NSEC_BADGE_YEAR 2026
#endif

// Build-time safety: CONFERENCE_ONLY and HAS_CHALLENGES must never both be set.
#if defined(CONFERENCE_ONLY) && defined(HAS_CHALLENGES)
#error "CONFERENCE_ONLY and HAS_CHALLENGES are mutually exclusive. Conference-only builds must not include CTF code."
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
inline constexpr UBaseType_t nfc_depth = 4;
inline constexpr UBaseType_t dock_depth = 8;
inline constexpr UBaseType_t display_depth = 4;
inline constexpr UBaseType_t portal_depth = 4;
}  // namespace queues

namespace tasks {
inline constexpr UBaseType_t priority_cli = 1;
inline constexpr UBaseType_t priority_display = 1;
inline constexpr UBaseType_t priority_portal = 1;
inline constexpr UBaseType_t priority_controller = 2;
inline constexpr UBaseType_t priority_led = 3;
inline constexpr UBaseType_t priority_nfc = 3;
inline constexpr UBaseType_t priority_dock = 1;
inline constexpr UBaseType_t priority_light = 1;
inline constexpr UBaseType_t priority_button = 5;
}  // namespace tasks

// NFC profile sharing field limits.
// The full profile (name + pronouns + affiliation + contact + length prefixes + RGB)
// must fit in a single NFC-DEP frame alongside the 38-byte auth payload (MAC + HMAC).
// Max DEP payload is 254 bytes, leaving ~210 bytes for the profile.
namespace profile {
inline constexpr uint8_t name_max_len = 20;
inline constexpr uint8_t pronouns_max_len = 10;
inline constexpr uint8_t affiliation_max_len = 30;
inline constexpr uint8_t contact_max_len = 60;
}  // namespace profile

namespace wifi {
inline constexpr uint8_t ssid_max_len = 15;    // "NSEC-XXYYZZ" (11 chars) fits; +1 for null in buffers
inline constexpr uint8_t passphrase_len = 10;  // alphanumeric chars (~59.5 bits entropy); WPA2 range: 8-63
}  // namespace wifi

}  // namespace badge::config
