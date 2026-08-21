#ifndef AURA_PERSONALITY_ENGINE_H
#define AURA_PERSONALITY_ENGINE_H

#include <Arduino.h>
#include <cstdint>
#include "config.h"
#include "sentence_generation_engine.h"

/**
 * @struct PersonalityKnobs
 * @brief Derived speaking-style controls for a personality profile.
 */
struct PersonalityKnobs {
    VocabStyle style;      ///< CASUAL / NEUTRAL / FORMAL register
    uint8_t humour;        ///< 0 (none) .. 2 (playful)
    uint8_t verbosity;     ///< 0 (minimal) .. 2 (rich / explanatory)
    uint8_t confidence;    ///< 0 (hedging) .. 2 (assertive)
    bool offersFollowUp;   ///< Whether follow-up questions are appended
};

/**
 * @class PersonalityEngine
 * @brief Expands the personality system for the offline Local AI Engine.
 *
 * Reads the active PersonalityManager profile (jarvis, professional,
 * programmer/developer, teacher, friendly, minimal — or any custom profile)
 * and converts it into concrete generation knobs: vocabulary register,
 * sentence length, confidence, greeting style, humour, formality and
 * response structure. All greeting/closing/transition text is delegated to
 * the SentenceGenerationEngine with the resolved register.
 */
class PersonalityEngine {
public:
    PersonalityEngine() noexcept;
    ~PersonalityEngine() noexcept;

    PersonalityEngine(const PersonalityEngine&) = delete;
    PersonalityEngine& operator=(const PersonalityEngine&) = delete;

    /** @brief Re-read the active profile and recompute knobs. */
    void refresh() noexcept;

    /** @brief True when PersonalityManager is available and initialized. */
    bool isActive() const noexcept;

    const char* profileId() const noexcept;
    const char* profileName() const noexcept;
    const PersonalityKnobs& knobs() const noexcept;

    /** @brief Short human label for the active style, e.g. "formal-friendly". */
    const char* styleName() const noexcept;

    // --- Register-aware fragment selection (delegates to SentenceEngine) ---
    String greeting() noexcept;
    String closing() noexcept;
    String transition() noexcept;
    String connective() noexcept;
    String possessiveVerb() noexcept;
    String adjective() noexcept;
    String ending() noexcept;
    String confidencePhrase(float confidence) noexcept;
    String synonym(const char* key) noexcept;

    /** @brief Wrap a data clause with a natural opening ("You currently have ..."). */
    String dataOpening(const char* noun, size_t count) noexcept;

private:
    void resolveKnobs(const String& profileId, const String& responseStyle) noexcept;

    bool m_active;
    char m_profileId[16];
    char m_profileName[16];
    char m_styleName[16];
    PersonalityKnobs m_knobs;
};

extern PersonalityEngine personalityEngine;

#endif // AURA_PERSONALITY_ENGINE_H
