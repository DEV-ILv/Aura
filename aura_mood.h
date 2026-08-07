#ifndef AURA_MOOD_H
#define AURA_MOOD_H

#include <Arduino.h>

/**
 * @file aura_mood.h
 * @brief Unified emotional/operational state shared by the OLED face and the
 *        WS2812B aura ring.
 *
 * AURA's face (OLED) and aura (RGB ring) are ONE system. A single AuraMood
 * drives both, so they always transition together.
 *
 * Moods are deliberately paired 1:1 with the product spec:
 *   Mood            OLED face          RGB aura
 *   -------------   ----------------   -------------------------
 *   IDLE            calm, breathing    LED ring OFF (idle = no light)
 *   LISTENING       wide, focused      deep blue sound wave (VU)
 *   THINKING        scanning           cyan energy flow
 *   PROCESSING      thinking continues soft cyan pulse
 *   SPEAKING        animated mouth     blue-white speech pulse
 *   HAPPY           big smile          warm gold expanding pulse
 *   SUCCESS         happy blink        green outward wave (then OFF)
 *   REMINDER        friendly           golden-yellow ripple
 *   WARNING         concerned          orange heartbeat
 *   ERROR           concerned          red slow breathing
 *   CRITICAL        serious            dark red slow pulse
 *   OTA             progress           purple rotating energy
 *   OFFLINE         calm soft eyes     LED ring OFF
 *   SLEEP           eyes close         LED ring OFF
 *   WAKE            eyes open          small expanding blue pulse (then OFF)
 *   BOOT            logo reveal        single LED fills ring (then OFF)
 *
 * Idle power policy: the aura ring is only lit for meaningful events and is
 * kept completely OFF while the system is idle (IDLE / SLEEP / OFFLINE).
 * Transient events (BOOT, SUCCESS, WAKE, WIFI_*) automatically return the
 * ring to OFF when they complete. See led_ring.h for the device-control
 * manual override used for LED tests, and docs/aura-led-state-machine.md.
 */
enum class AuraMood : uint8_t {
    BOOT = 0,
    IDLE,
    LISTENING,
    RECORDING,
    THINKING,
    PROCESSING,
    SPEAKING,
    HAPPY,
    SUCCESS,
    REMINDER,
    WARNING,
    ERROR,
    PRIVACY,
    CRITICAL,
    OTA,
    OFFLINE,
    SLEEP,
    WAKE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    SETUP,
    MAX
};

/**
 * @brief Human-readable name for a mood (debug/logging only).
 */
const char* auraMoodName(AuraMood mood) noexcept;

/**
 * @brief Global master brightness for each mood (0..255, spec percentages).
 *
 * Idle 20%, Listening 30%, Thinking 35%, Speaking 30%, Warning 25%,
 * Sleep 5%. Kept close to spec; FASTLED brightness is applied per frame.
 */
constexpr uint8_t auraMoodBrightness[static_cast<uint8_t>(AuraMood::MAX)] = {
    128,  // BOOT           (ramps up during boot sweep)
    51,   // IDLE           20%
    77,   // LISTENING      30%
    70,   // RECORDING       27%
    89,   // THINKING       35%
    82,   // PROCESSING     32%
    77,   // SPEAKING       30%
    71,   // HAPPY          28%
    77,   // SUCCESS        30%
    64,   // REMINDER       25%
    64,   // WARNING        25%
    64,   // ERROR          25%
    64,   // PRIVACY        25%
    46,   // CRITICAL       18%
    87,   // OTA            34%
    26,   // OFFLINE        10%
    13,   // SLEEP           5%
    66,   // WAKE           26%
    66,   // WIFI_CONNECTING 26%
    66,   // WIFI_CONNECTED  26%
    89,   // SETUP           35%
};

#endif  // AURA_MOOD_H
