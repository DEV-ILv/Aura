#ifndef AURA_LED_RING_H
#define AURA_LED_RING_H

#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "logger.h"

/**
 * @file led_ring.h
 * @brief Public interface for the AURA WS2812B LED ring controller.
 *
 * Provides non-blocking, state-driven LED animations for the AURA AI Desktop
 * Assistant running on ESP32 with the FastLED framework.
 */

/**
 * @enum LedState
 * @brief Defines the visual state currently rendered by the LED ring.
 */
enum class LedState : uint8_t {
    BOOT,
    READY,
    LISTENING,
    THINKING,
    SPEAKING,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    REMINDER,
    OTA_UPDATE,
    ERROR,
    SLEEP
};

/**
 * @class LedRing
 * @brief Controls the AURA WS2812B LED ring and its state-driven animations.
 *
 * Call initialize() once during startup, then call update() periodically from
 * the main loop or an appropriate FreeRTOS task.
 */
class LedRing {
public:
    /**
     * @brief Creates an LED ring controller with its default state.
     */
    LedRing() noexcept;

    /**
     * @brief Destroys the LED ring controller.
     */
    ~LedRing() = default;

    LedRing(const LedRing&) = delete;
    LedRing& operator=(const LedRing&) = delete;
    LedRing(LedRing&&) = delete;
    LedRing& operator=(LedRing&&) = delete;

    /**
     * @brief Initializes FastLED and configures the LED ring.
     *
     * Must be called once before update() or any state transition.
     */
    void initialize() noexcept;

    /**
     * @brief Advances and renders the active non-blocking animation.
     *
     * Call periodically from the main execution loop or a dedicated task.
     */
    void update() noexcept;

    /**
     * @brief Selects the animation associated with an LED state.
     * @param newState Target state to render.
     */
    void setState(LedState newState) noexcept;

    /**
     * @brief Gets the currently active LED state.
     * @return Active LED state.
     */
    [[nodiscard]] LedState getState() const noexcept;

    /**
     * @brief Sets the global FastLED brightness.
     * @param brightness Brightness value in the range 0 to 255.
     */
    void setBrightness(uint8_t brightness) noexcept;

    /**
     * @brief Sets the firmware update progress for the OTA animation.
     * @param percentage Completion percentage in the range 0 to 100.
     */
    void setOtaProgress(uint8_t percentage) noexcept;

    /**
     * @brief Set a personality theme color that overrides default state colors.
     * @param color RGB color (0 = use defaults)
     */
    void setThemeColor(CRGB color) noexcept;

    /**
     * @brief Disables output, clears the LED buffer, and turns off the ring.
     */
    void turnOff() noexcept;

    /**
     * @brief Enables output and resumes the current state animation.
     */
    void turnOn() noexcept;

    /**
     * @brief Checks whether LED output is enabled.
     * @return True when the LED ring is enabled; otherwise false.
     */
    [[nodiscard]] bool isEnabled() const noexcept;

    /**
     * @brief Get current theme color
     * @return CRGB theme color (CRGB::Black = use defaults)
     */
    [[nodiscard]] CRGB getThemeColor() const noexcept;

private:
    LedState m_currentState;              ///< Currently active LED state.
    uint8_t m_brightness;                 ///< Global FastLED brightness.
    bool m_isEnabled;                     ///< Controls LED output.
    uint8_t m_otaProgress;                ///< OTA completion percentage.
    uint32_t m_animationFrame;            ///< Current animation frame.
    unsigned long m_animationTimer;       ///< Last animation update timestamp.
    CRGB m_leds[LED_COUNT];               ///< FastLED pixel buffer.
    CRGB m_themeColor;                    ///< Personality theme color override

    /**
     * @brief Renders the boot animation.
     */
    void playBootAnimation() noexcept;

    /**
     * @brief Renders the ready-state animation.
     */
    void playReadyAnimation() noexcept;

    /**
     * @brief Renders the listening-state animation.
     */
    void playListeningAnimation() noexcept;

    /**
     * @brief Renders the thinking-state animation.
     */
    void playThinkingAnimation() noexcept;

    /**
     * @brief Renders the speaking-state animation.
     */
    void playSpeakingAnimation() noexcept;

    /**
     * @brief Renders the Wi-Fi connection animation.
     */
    void playWifiConnectingAnimation() noexcept;

    /**
     * @brief Renders the Wi-Fi connected confirmation animation.
     */
    void playWifiConnectedAnimation() noexcept;

    /**
     * @brief Renders the reminder notification animation.
     */
    void playReminderAnimation() noexcept;

    /**
     * @brief Renders the OTA update progress animation.
     */
    void playOtaUpdateAnimation() noexcept;

    /**
     * @brief Renders the error-state animation.
     */
    void playErrorAnimation() noexcept;

    /**
     * @brief Renders the low-power sleep animation.
     */
    void playSleepAnimation() noexcept;

    /**
     * @brief Select the effective color for a state (theme override if set).
     * @param defaultColor The default CRGB for this state.
     * @return m_themeColor if not Black, otherwise defaultColor.
     */
    CRGB resolveColor(CRGB defaultColor) const noexcept;

    /**
     * @brief Clears the LED buffer and updates the physical ring.
     */
    void clearRing() noexcept;

    /**
     * @brief Resets frame and timing data for a state transition.
     */
    void resetAnimation() noexcept;
};

/**
 * @brief Global LED ring controller instance.
 */
extern LedRing ledRing;
#endif  // AURA_LED_RING_H