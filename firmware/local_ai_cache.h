#ifndef AURA_LOCAL_AI_CACHE_H
#define AURA_LOCAL_AI_CACHE_H

#include <Arduino.h>
#include <cstdint>
#include "config.h"

/**
 * @struct CacheSlot
 * @brief One cached question/answer pair.
 */
struct CacheSlot {
    String question;
    String answer;
};

/**
 * @class LocalAICache
 * @brief Small in-RAM LRU cache for the Local AI Engine.
 *
 * Caches recent questions/responses for fast identical-repeat answers,
 * remembers frequently used phrases, and tracks the last emitted response so
 * the generator can guarantee it never repeats identical wording in a row.
 * Bounded and allocation-light.
 */
class LocalAICache {
public:
    static constexpr size_t kCapacity = LOCAL_AI_CACHE_SIZE;

    LocalAICache() noexcept;
    ~LocalAICache() noexcept;

    LocalAICache(const LocalAICache&) = delete;
    LocalAICache& operator=(const LocalAICache&) = delete;

    /** @brief Look up an exact question. Returns true on hit. */
    bool lookup(const String& question, String& answer) noexcept;

    /** @brief Store a question/answer pair (evicting oldest on overflow). */
    void remember(const String& question, const String& answer) noexcept;

    /** @brief Record that a specific response text was just emitted. */
    void noteResponse(const String& answer) noexcept;

    /** @brief True when the given text was the most recently emitted response. */
    bool isRecentResponse(const String& answer) const noexcept;

    /** @brief True when the phrase was used recently (for vocabulary reuse). */
    bool isFrequentPhrase(const String& phrase) const noexcept;

    size_t size() const noexcept;
    size_t hitCount() const noexcept;
    void reset() noexcept;

private:
    CacheSlot m_slots[kCapacity];
    size_t m_count;
    size_t m_oldest;
    size_t m_hits;
    String m_lastResponse;
};

extern LocalAICache localAICache;

#endif // AURA_LOCAL_AI_CACHE_H
