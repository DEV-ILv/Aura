#ifndef AURA_LED_RING_H
#define AURA_LED_RING_H

#include <Arduino.h>
#include <FastLED.h>

#include "aura_mood.h"
#include "config.h"
#include "logger.h"

/**
 * @file led_ring.h
 * @brief Public interface for the AURA WS2812B aura ring.
 *
 * The ring is A.U.R.A's "aura" and normally mirrors the OLED face mood. It is
 * driven from the single AuraMood state (see aura_mood.h) so the two can
 * never drift apart. Animations are non-blocking, timer-driven and rendered
 * in update() from the main loop or a FreeRTOS task.
 *
 * Idle power policy: the ring is ONLY lit for meaningful events (boot,
 * listening, thinking/processing, speaking, OTA, notifications, warnings,
 * errors, brief Wi-Fi/BT status). Whenever the system is idle
 * (AuraMood::IDLE / SLEEP / OFFLINE) the ring is kept fully OFF. Events
 * return to IDLE through the normal AuraMood transitions, at which point the
 * ring automatically switches off again.
 *
 * Manual override: the device-control page can drive the ring directly
 * (on/off, brightness, colour, animation). Manual commands open a temporary
 * manual session that is either closed explicitly (control disabled) or
 * auto-expires after kManualControlTimeoutMs of inactivity, after which the
 * ring returns to automatic behaviour (off while idle).
 */
class LedRing {
public:
    /// Automatic (follows the AuraMood state) vs manual (device-control test).
    enum class RingControlMode : uint8_t {
        kAutomatic = 0,
        kManual,
    };

    LedRing() noexcept;
    ~LedRing() = default;

    LedRing(const LedRing&) = delete;
    LedRing& operator=(const LedRing&) = delete;
    LedRing(LedRing&&) = delete;
    LedRing& operator=(LedRing&&) = delete;

    /**
     * @brief Configures FastLED and prepares the ring. Call once at startup.
     */
    void initialize() noexcept;

    /**
     * @brief Advances and renders the active mood animation. Non-blocking.
     */
    void update() noexcept;

    /**
     * @brief Selects the mood rendered by the ring (single source of truth).
     */
    void setMood(AuraMood mood) noexcept;

    /**
     * @brief Gets the mood currently rendered.
     */
    [[nodiscard]] AuraMood getMood() const noexcept;

    /**
     * @brief Feeds a live voice/mic level (0..255) used by the VU-driven moods.
     */
    void setVoiceLevel(uint8_t level) noexcept;

    /**
     * @brief Sets OTA completion percentage (0..100) for the OTA mood.
     */
    void setOtaProgress(uint8_t percentage) noexcept;

    /**
     * @brief Sets the global ring brightness (0..255).
     */
    void setBrightness(uint8_t brightness) noexcept;

    /**
     * @brief Personality theme colour that tints the active mood.
     * @param color CRGB; CRGB::Black keeps the mood's default colours.
     */
    void setThemeColor(CRGB color) noexcept;

    /**
     * @brief Returns the active theme colour.
     */
    [[nodiscard]] CRGB getThemeColor() const noexcept;

    /**
     * @brief Subscribes to VAD events so the ring lights green immediately
     *        when voice is detected, before any STT/AI pipeline runs.
     */
    void subscribeToEvents() noexcept;

    /**
     * @brief Turns the ring off (buffer cleared, output disabled).
     */
    void turnOff() noexcept;

    /**
     * @brief Turns the ring back on, resuming the active mood.
     */
    void turnOn() noexcept;

    /**
     * @brief Whether output is enabled.
     */
    [[nodiscard]] bool isEnabled() const noexcept;

    // ---- manual device-control session -------------------------------------
    //
    // Manual sessions let the Device Control page drive the ring directly for
    // tests. They are event-driven: each command refreshes the session, and it
    // auto-expires after kManualControlTimeoutMs of inactivity (no polling
    // loops, no delay()-based timers). On exit the ring returns to automatic
    // behaviour, which means fully OFF while the system is idle.

    /**
     * @brief Opens or refreshes a manual control session.
     *
     * The first call snapshots the current system mood as the starting
     * animation so colour/brightness changes are immediately visible.
     */
    void beginManualControl() noexcept;

    /**
     * @brief Closes the manual session and restores automatic behaviour.
     *
     * Safe to call when no manual session is active.
     */
    void endManualControl() noexcept;

    /**
     * @brief Sets the animation shown during a manual session.
     * @param mood Any AuraMood; ignored outside a manual session.
     */
    void setManualMood(AuraMood mood) noexcept;

    /**
     * @brief Gets the mood shown during a manual session.
     */
    [[nodiscard]] AuraMood getManualMood() const noexcept;

    /**
     * @brief Whether a manual control session is currently active.
     */
    [[nodiscard]] bool isManualControl() const noexcept;

    // ---- disco mode --------------------------------------------------------
    //
    // Disco Mode is a fun, app-only override: while enabled the ring runs a
    // rotating set of smooth (non-blocking, FreeRTOS-safe) colour animations
    // at the disco brightness instead of the normal mood animation. It never
    // activates automatically - only from the Companion App. Emergency moods
    // (Error / OTA / Critical) always take priority and pause it; when the
    // emergency clears, Disco Mode resumes if it is still enabled. The
    // enabled flag is intentionally NOT persisted: Disco Mode is OFF by
    // default after every reboot.

    /**
     * @brief Enable or disable Disco Mode.
     * @return true if the mode state changed.
     */
    bool setDiscoEnabled(bool enabled) noexcept;

    /**
     * @brief Whether Disco Mode is enabled (regardless of emergency pause).
     */
    [[nodiscard]] bool isDiscoEnabled() const noexcept;

    /**
     * @brief Whether Disco Mode is currently being rendered (enabled and not
     *        paused by an emergency mood).
     */
    [[nodiscard]] bool isDiscoActive() const noexcept;

    /**
     * @brief Sets Disco Mode brightness in percent (clamped 10..100).
     */
    void setDiscoBrightness(uint8_t percent) noexcept;

    /**
     * @brief Returns the Disco Mode brightness in percent (10..100).
     */
    [[nodiscard]] uint8_t getDiscoBrightness() const noexcept;

private:
    AuraMood m_mood;                  ///< Active system mood.
    AuraMood m_manualMood;            ///< Mood shown during a manual session.
    RingControlMode m_controlMode;    ///< Automatic vs manual override.
    unsigned long m_manualSince;      ///< Last manual command tick (ms).
    uint8_t m_brightness;             ///< Global FastLED brightness.
    bool m_isEnabled;                 ///< Output enabled.
    bool m_ledsBlack;                 ///< Buffer is all-black (skip re-shows).
    bool m_subscribed;                ///< Event-bus VAD subscription installed.
    uint8_t m_voiceLevel;             ///< Live mic energy 0..255.
    uint8_t m_otaProgress;            ///< OTA completion %.
    uint32_t m_animationFrame;        ///< Frame counter.
    unsigned long m_animationTimer;   ///< Last animation tick.
    unsigned long m_moodSince;        ///< When the current mood began.
    CRGB m_leds[LED_COUNT];           ///< FastLED pixel buffer (persistent, blended).
    CRGB m_frame[LED_COUNT];          ///< Freshly computed target frame.
    CRGB m_themeColor;                ///< Personality tint.

    // ---- disco mode --------------------------------------------------------
    bool          m_discoEnabled;     ///< Disco Mode enabled (app only).
    uint8_t       m_discoBrightness;  ///< Disco brightness percent (10..100).
    uint8_t       m_discoAnim;        ///< Active disco animation index.
    unsigned long m_discoAnimSince;   ///< When the current disco animation began.
    unsigned long m_discoAnimDuration;///< Random duration until the next animation.

    // ---- idle micro-sparkle (manual idle mood only) -------------------------
    unsigned long m_sparkleNext;
    unsigned long m_sparkleUntil;
    bool          m_sparkleActive;
    uint8_t       m_sparkleHead;

    // ---- whole-ring brightness flicker (solid status path only) -------------
    uint8_t       m_flickerLevel;     ///< Current shared brightness level (0..255).
    uint8_t       m_flickerTarget;    ///< Random-walk target brightness level.
    unsigned long m_flickerNext;      ///< Next recompute timestamp (ms).

    // ---- animation renderers (one per mood) --------------------------------
    void playBoot() noexcept;
    void playIdle() noexcept;
    void playListening() noexcept;
    void playRecording() noexcept;
    void playThinking() noexcept;
    void playProcessing() noexcept;
    void playSpeaking() noexcept;
    void playHappy() noexcept;
    void playSuccess() noexcept;
    void playReminder() noexcept;
    void playWarning() noexcept;
    void playError() noexcept;
    void playPrivacy() noexcept;
    void playCritical() noexcept;
    void playOta() noexcept;
    void playOffline() noexcept;
    void playSleep() noexcept;
    void playWake() noexcept;
    void playWifiConnecting() noexcept;
    void playWifiConnected() noexcept;
    void playSetup() noexcept;

    // ---- helpers ------------------------------------------------------------
    CRGB moodColor(CRGB base) const noexcept;
    [[nodiscard]] uint8_t effectiveBrightness() const noexcept;
    [[nodiscard]] bool isQuietMood(AuraMood mood) const noexcept;
    [[nodiscard]] bool isEmergencyMood(AuraMood mood) const noexcept;
    void clearRing() noexcept;
    void easeBrightness() noexcept;
    void renderDisco() noexcept;

    // ---- normal SOLID status system -----------------------------------------
    //
    // The 16-LED ring is rendered as ONE indicator: every normal AURA state
    // fills ALL 16 LEDs with a single distinct SOLID colour (no per-LED
    // movement, chase, comet, tail, pulse, breathe, or rotation). A subtle
    // whole-ring, synchronised brightness flicker keeps the ring "alive".
    // This is the normal, state-machine-driven status path and is fully
    // independent from Disco Mode.
    [[nodiscard]] bool isSequentialAnimation(AuraMood mood) const noexcept;
    void renderSolidStatus(const CRGB color, uint8_t loPercent, uint8_t hiPercent,
                           uint8_t stepMax, uint8_t intervalMinMs, uint8_t intervalMaxMs) noexcept;

    // ---- disco animations (one per rotating effect) -------------------------
    void playDiscoRainbowRotate() noexcept;
    void playDiscoRainbowSpiral() noexcept;
    void playDiscoColorWipe() noexcept;
    void playDiscoTheaterChase() noexcept;
    void playDiscoRainbowBreath() noexcept;
    void playDiscoFire() noexcept;
    void playDiscoOcean() noexcept;
    void playDiscoAurora() noexcept;
    void playDiscoSparkle() noexcept;
    void playDiscoPulse() noexcept;
};

/**
 * @brief Global aura ring controller.
 */
extern LedRing ledRing;
#endif  // AURA_LED_RING_H
