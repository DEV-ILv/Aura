#ifndef AURA_BRIEFING_MANAGER_H
#define AURA_BRIEFING_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"

// ========================================================================
// Briefing types (existing)
// ========================================================================

struct BriefingRecord {
    String id;
    unsigned long timestamp;
    String type;
    String content;

    BriefingRecord() noexcept : timestamp(0) {}
};

// ========================================================================
// DailySummary types (merged from DailySummaryManager)
// ========================================================================

struct DailySummary {
    String id;
    String date;
    unsigned long timestamp;
    String content;
    size_t reminderCount;
    size_t memoryCount;
    size_t conversationCount;
    bool archived;
    bool favorite;

    DailySummary() noexcept
        : timestamp(0), reminderCount(0), memoryCount(0),
          conversationCount(0), archived(false), favorite(false) {}
};

// ========================================================================
// BriefingManager
// ========================================================================

class BriefingManager {
public:
    BriefingManager() noexcept;
    ~BriefingManager() noexcept;

    BriefingManager(const BriefingManager&) = delete;
    BriefingManager& operator=(const BriefingManager&) = delete;
    BriefingManager(BriefingManager&&) = delete;
    BriefingManager& operator=(BriefingManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // --- Briefing API ---
    [[nodiscard]] String generateMorning() noexcept;
    [[nodiscard]] String generateEvening() noexcept;
    [[nodiscard]] bool saveBriefings() noexcept;
    [[nodiscard]] bool loadBriefings() noexcept;

    // --- DailySummary API ---
    [[nodiscard]] bool generateTodaySummary() noexcept;
    [[nodiscard]] DailySummary getTodaySummary() const noexcept;
    [[nodiscard]] const std::vector<DailySummary>& getAllSummaries() const noexcept;
    [[nodiscard]] bool deleteSummary(const String& summaryId) noexcept;
    [[nodiscard]] bool saveSummaries() noexcept;
    [[nodiscard]] bool loadSummaries() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "BriefingMgr";
    static constexpr size_t kMaxRecords = 90;

    // --- Briefing internals ---
    String generateId() noexcept;
    String buildMorning() noexcept;
    String buildEvening() noexcept;
    bool saveBriefingRecord(const String& type, const String& content) noexcept;

    // --- DailySummary internals ---
    static constexpr size_t kMaxSummaries = 90;
    static constexpr const char* kSummariesPath = "/cache/daily_summaries.json";

    String generateSummaryId() noexcept;
    String getTodayDate() const noexcept;
    size_t findSummary(const String& id) const noexcept;

    // --- Briefing state ---
    bool m_initialized;
    bool m_briefingsDirty;
    std::vector<BriefingRecord> m_records;
    unsigned long m_lastIdCounter;
    unsigned long m_lastGenerateTime;

    // --- DailySummary state ---
    bool m_summariesDirty;
    std::vector<DailySummary> m_summaries;
};

extern BriefingManager briefingManager;

#endif
