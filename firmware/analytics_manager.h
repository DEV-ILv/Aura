#ifndef AURA_ANALYTICS_MANAGER_H
#define AURA_ANALYTICS_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "storage_manager.h"

struct AnalyticsRecord {
    unsigned long id;
    unsigned long timestamp;
    String category;
    String metric;
    float value;
    String unit;
    String tags;
};

struct AnalyticsTrend {
    String metric;
    float slope;
    float average;
    float min;
    float max;
    size_t dataPoints;
    bool increasing;
};

struct AnalyticsSummary {
    unsigned long dateKey;
    float studyHours;
    size_t remindersCompleted;
    size_t tasksCompleted;
    size_t conversationsHad;
    size_t knowledgeFactsAdded;
    size_t projectsWorkedOn;
    float systemUptimeHours;
    float productivityScore;
};

class AnalyticsManager {
public:
    AnalyticsManager() noexcept;
    ~AnalyticsManager() noexcept;

    AnalyticsManager(const AnalyticsManager&) = delete;
    AnalyticsManager& operator=(const AnalyticsManager&) = delete;
    AnalyticsManager(AnalyticsManager&&) = delete;
    AnalyticsManager& operator=(AnalyticsManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    void record(const String& category, const String& metric, float value, const String& unit = "", const String& tags = "") noexcept;

    [[nodiscard]] std::vector<AnalyticsRecord> getRecords(const String& category, const String& metric, size_t limit = 100) noexcept;
    [[nodiscard]] AnalyticsTrend getTrend(const String& category, const String& metric) noexcept;
    [[nodiscard]] AnalyticsSummary getDailySummary(unsigned long dateKey) noexcept;
    [[nodiscard]] AnalyticsSummary getWeeklySummary(unsigned long weekKey) noexcept;
    [[nodiscard]] AnalyticsSummary getMonthlySummary(unsigned long monthKey) noexcept;

    [[nodiscard]] float getTotalStudyMinutes() noexcept;
    [[nodiscard]] float getReminderCompletionRate() noexcept;
    [[nodiscard]] size_t getTotalConversations() noexcept;
    [[nodiscard]] size_t getTotalKnowledgeFacts() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "AnalyticsManager";
    static constexpr size_t kMaxRecords = 2048;
    static constexpr unsigned long kSaveIntervalMs = 60000;

    void aggregateDaily() noexcept;
    unsigned long dateKeyFromTimestamp(unsigned long ts) noexcept;

    bool m_initialized;
    std::vector<AnalyticsRecord> m_records;
    unsigned long m_lastSaveTime;
    unsigned long m_lastAggregateTime;
};

extern AnalyticsManager analyticsManager;

#endif // AURA_ANALYTICS_MANAGER_H
