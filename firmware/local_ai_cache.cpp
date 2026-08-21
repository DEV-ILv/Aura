#include "local_ai_cache.h"

LocalAICache localAICache;

LocalAICache::LocalAICache() noexcept
    : m_count(0), m_oldest(0), m_hits(0) {}

LocalAICache::~LocalAICache() noexcept {}

bool LocalAICache::lookup(const String& question, String& answer) noexcept {
    for (size_t i = 0; i < m_count; ++i) {
        if (m_slots[i].question.equalsIgnoreCase(question)) {
            answer = m_slots[i].answer;
            m_hits++;
            return true;
        }
    }
    return false;
}

void LocalAICache::remember(const String& question, const String& answer) noexcept {
    // Update in place if the question already exists (keeps MRU-ish).
    for (size_t i = 0; i < m_count; ++i) {
        if (m_slots[i].question.equalsIgnoreCase(question)) {
            m_slots[i].answer = answer;
            return;
        }
    }

    if (m_count < kCapacity) {
        m_slots[m_count].question = question;
        m_slots[m_count].answer = answer;
        m_count++;
        return;
    }

    // Evict oldest (simple FIFO ring).
    m_slots[m_oldest].question = question;
    m_slots[m_oldest].answer = answer;
    m_oldest = (m_oldest + 1) % kCapacity;
}

void LocalAICache::noteResponse(const String& answer) noexcept {
    m_lastResponse = answer;
}

bool LocalAICache::isRecentResponse(const String& answer) const noexcept {
    if (m_lastResponse.isEmpty()) return false;
    return m_lastResponse.equalsIgnoreCase(answer);
}

bool LocalAICache::isFrequentPhrase(const String& phrase) const noexcept {
    for (size_t i = 0; i < m_count; ++i) {
        if (m_slots[i].answer.indexOf(phrase) >= 0) return true;
    }
    return false;
}

size_t LocalAICache::size() const noexcept {
    return m_count;
}

size_t LocalAICache::hitCount() const noexcept {
    return m_hits;
}

void LocalAICache::reset() noexcept {
    for (size_t i = 0; i < kCapacity; ++i) {
        m_slots[i] = CacheSlot();
    }
    m_count = 0;
    m_oldest = 0;
    m_hits = 0;
    m_lastResponse = "";
}
