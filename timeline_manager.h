#ifndef AURA_TIMELINE_MANAGER_H
#define AURA_TIMELINE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct TimelineEntry {
    String id;
    unsigned long timestamp;
    unsigned long dateKey;
    String category;
    String summary;
    uint8_t importance;
    String linkedMemoryIds;
    String linkedGoalIds;
    String linkedConversationIds;
    String linkedGraphNodeIds;

    TimelineEntry() noexcept : timestamp(0), dateKey(0), importance(0) {}
};

enum class TimelineCategory : uint8_t {
    GOAL_CREATED,
    GOAL_COMPLETED,
    REMINDER_COMPLETED,
    CONVERSATION,
    MEMORY_SAVED,
    HABIT_COMPLETED,
    PROJECT_STARTED,
    PROJECT_FINISHED,
    PLUGIN_INSTALLED,
    OTA_UPDATE,
    STUDY_SESSION,
    CUSTOM
};

class TimelineManager {
public:
    TimelineManager() noexcept;
    ~TimelineManager() noexcept;

    TimelineManager(const TimelineManager&) = delete;
    TimelineManager& operator=(const TimelineManager&) = delete;
    TimelineManager(TimelineManager&&) = delete;
    TimelineManager& operator=(TimelineManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String addEntry(TimelineCategory category, const String& summary,
                                    uint8_t importance = 1,
                                    const String& linkedMemories = "",
                                    const String& linkedGoals = "",
                                    const String& linkedConversations = "",
                                    const String& linkedGraphNodes = "") noexcept;

    [[nodiscard]] std::vector<TimelineEntry> getToday() const noexcept;
    [[nodiscard]] std::vector<TimelineEntry> getYesterday() const noexcept;
    [[nodiscard]] std::vector<TimelineEntry> getThisWeek() const noexcept;
    [[nodiscard]] std::vector<TimelineEntry> getThisMonth() const noexcept;
    [[nodiscard]] std::vector<TimelineEntry> getRange(unsigned long startDate, unsigned long endDate) const noexcept;
    [[nodiscard]] std::vector<TimelineEntry> search(const String& query) const noexcept;

    [[nodiscard]] size_t entryCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "TimelineMgr";
    static constexpr size_t kMaxEntries = TIMELINE_MAX_ENTRIES;
    static constexpr size_t kBatchSize = TIMELINE_BATCH_SIZE;

    String generateId() noexcept;
    unsigned long computeDateKey(unsigned long ts) const noexcept;
    const char* categoryToString(TimelineCategory c) const noexcept;
    TimelineCategory stringToCategory(const String& s) const noexcept;
    void trimToMax() noexcept;
    String serializeEntry(const TimelineEntry& e) const noexcept;
    TimelineEntry deserializeEntry(const String& json) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<TimelineEntry> m_entries;
    unsigned long m_lastIdCounter;
    unsigned long m_lastBatchSave;
};

extern TimelineManager timelineManager;

#endif
