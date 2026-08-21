#ifndef AURA_SENTENCE_GENERATION_ENGINE_H
#define AURA_SENTENCE_GENERATION_ENGINE_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include "config.h"

/**
 * @enum VocabStyle
 * @brief Tonal register used when selecting fragments.
 */
enum class VocabStyle : uint8_t {
    CASUAL,
    NEUTRAL,
    FORMAL
};

/**
 * @class SentenceGenerationEngine
 * @brief Micro language engine that assembles responses from reusable
 *        flash-resident fragments (verbs, connectors, phrases, greetings,
 *        closings, adjectives, synonyms, transitions, endings).
 *
 * Stores fragments instead of complete sentences to minimise flash usage,
 * and always selects variants at runtime so the same reply is never emitted
 * twice in a row.
 */
class SentenceGenerationEngine {
public:
    SentenceGenerationEngine() noexcept;
    ~SentenceGenerationEngine() noexcept;

    SentenceGenerationEngine(const SentenceGenerationEngine&) = delete;
    SentenceGenerationEngine& operator=(const SentenceGenerationEngine&) = delete;

    // ========================================================================
    // Fragment selection (with immediate-repeat avoidance)
    // ========================================================================

    const char* pickGreeting(VocabStyle style) noexcept;
    const char* pickClosing(VocabStyle style) noexcept;
    const char* pickTransition(VocabStyle style) noexcept;
    const char* pickConnector() noexcept;
    const char* pickPossessiveVerb(VocabStyle style) noexcept;
    const char* pickAdjective(VocabStyle style) noexcept;
    const char* pickEnding(VocabStyle style) noexcept;

    /** @brief Confidence phrase for a 0..1 confidence value. */
    const char* pickConfidence(float confidence) noexcept;

    /** @brief Synonym pool for a common noun key ("reminder","task","goal",...). */
    const char* pickSynonym(const char* key) noexcept;

    /** @brief Generic random pick from any pool. */
    const char* pickFrom(const char* const* pool, size_t count) noexcept;

    // ========================================================================
    // Composition helpers
    // ========================================================================

    /** @brief "3" -> "three" (0..20 supported, larger falls back to digits). */
    String numberWord(size_t n) const noexcept;

    /** @brief Singular/plural noun phrase with count word, e.g. "three reminders". */
    String countPhrase(size_t n, const char* noun) const noexcept;

    /** @brief Join a vector of strings. */
    String join(const std::vector<String>& items, const char* separator,
                size_t maxItems = LOCAL_AI_MAX_DATA_ITEMS) const noexcept;

    /** @brief List items in natural form: "a, b and c". */
    String listItems(const std::vector<String>& items,
                     size_t maxItems = LOCAL_AI_MAX_DATA_ITEMS) const noexcept;

    /** @brief Capitalise first character of a string. */
    String capitalise(const String& s) const noexcept;

    /** @brief Generic "this is X" acknowledgement with a synonym + adjective. */
    String acknowledgement(const String& noun, VocabStyle style) noexcept;

private:
    static const char* const kGreetings[3][5];
    static const char* const kClosings[3][5];
    static const char* const kTransitions[3][5];
    static const char* const kVerbs[3][4];
    static const char* const kAdjectives[3][6];
    static const char* const kEndings[3][5];
    static const char* const kConnectors[6];
    static const char* const kConfidenceHigh[3];
    static const char* const kConfidenceMed[3];
    static const char* const kConfidenceLow[3];
    static const char* const kNumberWords[21];
    static const char* const kSynonyms[6][4];

    uint8_t m_lastGreeting;
    uint8_t m_lastClosing;
    uint8_t m_lastTransition;
    uint8_t m_lastConnector;
};

extern SentenceGenerationEngine sentenceEngine;

#endif // AURA_SENTENCE_GENERATION_ENGINE_H
