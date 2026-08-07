#ifndef AURA_SYSTEM_H
#define AURA_SYSTEM_H

#include <Arduino.h>

#include "aura_mood.h"
#include "aura_face.h"
#include "led_ring.h"
#include "logger.h"

/**
 * @file aura_system.h
 * @brief Single coordinator for AURA's emotional presence.
 *
 * The OLED face and the WS2812B aura ring are ONE system. AuraSystem is the
 * only entry point for mood changes: it always drives the face expression and
 * the ring animation together, so they can never behave independently.
 */
class AuraSystem {
public:
    AuraSystem() noexcept = default;
    ~AuraSystem() = default;

    AuraSystem(const AuraSystem&) = delete;
    AuraSystem& operator=(const AuraSystem&) = delete;

    /**
     * @brief Initialises the aura system. Call once after display/ring ready.
     */
    void initialize() noexcept;

    /**
     * @brief Advances the aura system (drives the ring). Call from the loop.
     */
    void update() noexcept;

    /**
     * @brief Set the current mood. Face + ring transition together.
     */
    void setMood(AuraMood mood) noexcept;

    /**
     * @brief Get the active mood.
     */
    [[nodiscard]] AuraMood getMood() const noexcept { return m_mood; }

    /**
     * @brief Feed live mic energy (0..255) to the face + VU ring moods.
     */
    void setVoiceLevel(uint8_t level) noexcept;

    /**
     * @brief Forward OTA progress (0..100) to the ring.
     */
    void setOtaProgress(uint8_t percentage) noexcept;

    // ------------------------------------------------------------------
    // High-level helpers (readable call sites, still fully synchronised)
    // ------------------------------------------------------------------
    void enterIdle() noexcept          { setMood(AuraMood::IDLE); }
    void listen() noexcept             { setMood(AuraMood::LISTENING); }
    void record() noexcept             { setMood(AuraMood::RECORDING); }
    void think() noexcept              { setMood(AuraMood::THINKING); }
    void process() noexcept            { setMood(AuraMood::PROCESSING); }
    void speak() noexcept              { setMood(AuraMood::SPEAKING); }
    void happy() noexcept              { setMood(AuraMood::HAPPY); }
    void success() noexcept            { setMood(AuraMood::SUCCESS); }
    void reminder() noexcept           { setMood(AuraMood::REMINDER); }
    void warning() noexcept            { setMood(AuraMood::WARNING); }
    void error() noexcept              { setMood(AuraMood::ERROR); }
    void privacy() noexcept            { setMood(AuraMood::PRIVACY); }
    void critical() noexcept           { setMood(AuraMood::CRITICAL); }
    void offline() noexcept            { setMood(AuraMood::OFFLINE); }
    void sleep() noexcept              { setMood(AuraMood::SLEEP); }
    void wake() noexcept               { setMood(AuraMood::WAKE); }

private:
    void applyFace(AuraMood mood) noexcept;

    AuraMood m_mood = AuraMood::BOOT;
    bool m_initialized = false;
};

/**
 * @brief Global aura system coordinator.
 */
extern AuraSystem auraSystem;

#endif  // AURA_SYSTEM_H
