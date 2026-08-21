#ifndef AURA_REFLECTION_MANAGER_H
#define AURA_REFLECTION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct ReflectionRecord {
    String id;
    String date;
    unsigned long timestamp;
    size_t conversationsSummarized;
    size_t memoriesExtracted;
    size_t temporariesRemoved;
    size_t duplicatesMerged;
    size_t graphLinksAdded;
    float productivityScore;
    String summary;

    ReflectionRecord() noexcept : timestamp(0), conversationsSummarized(0),
        memoriesExtracted(0), temporariesRemoved(0), duplicatesMerged(0),
        graphLinksAdded(0), productivityScore(0.0f) {}
};

class ReflectionManager {
public:
    ReflectionManager() noexcept;
    ~ReflectionManager() noexcept;

    ReflectionManager(const ReflectionManager&) = delete;
    ReflectionManager& operator=(const ReflectionManager&) = delete;
    ReflectionManager(ReflectionManager&&) = delete;
    ReflectionManager& operator=(ReflectionManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    /**
     * @brief Run nightly reflection cycle
     * @return ReflectionRecord with results
     */
    [[nodiscard]] ReflectionRecord runReflection() noexcept;

    /**
     * @brief Get the latest reflection record
     * @return ReflectionRecord
     */
    [[nodiscard]] ReflectionRecord getLatestReflection() const noexcept;

    /**
     * @brief Get all reflection history
     * @return Vector of reflection records
     */
    [[nodiscard]] const std::vector<ReflectionRecord>& getHistory() const noexcept;

    [[nodiscard]] size_t reflectionCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "ReflectionManager";
    static constexpr size_t kMaxHistory = REFLECTION_MAX_HISTORY;

    String generateDate() const noexcept;
    String generateId() noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<ReflectionRecord> m_history;
    unsigned long m_lastRunDay;
    unsigned long m_lastIdCounter;
};

extern ReflectionManager reflectionManager;

#endif
