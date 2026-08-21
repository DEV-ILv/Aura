#ifndef AURA_EXECUTIVE_ASSISTANT_H
#define AURA_EXECUTIVE_ASSISTANT_H

#include <Arduino.h>
#include <vector>

// ========================================================================
// Suggestion types (existing)
// ========================================================================

enum class SuggestionCategory : uint8_t {
    STUDY_REMINDER,
    PROJECT_UPDATE,
    REMINDER_UPCOMING,
    WIFI_ISSUE,
    STORAGE_ISSUE,
    BACKUP_REMINDER,
    HABIT_REMINDER,
    FOCUS_SUGGESTION,
    ROUTINE_SUGGESTION,
    KNOWLEDGE_REVIEW,
    PRODUCTIVITY_INSIGHT,
    SYSTEM_HEALTH,
    DAILY_BRIEF,
    CUSTOM
};

struct Suggestion {
    String id;
    String title;
    String description;
    uint8_t priority;
    unsigned long timestamp;
    bool actionable;
    bool shown;
    unsigned long cooldownEnd;
    SuggestionCategory category;
};

// ========================================================================
// Recommendation types (merged from RecommendationManager)
// ========================================================================

enum class RecommendationCategory : uint8_t {
    STUDY_SUGGESTION,
    BREAK_REMINDER,
    PROJECT_CONTINUATION,
    MEMORY_REVIEW,
    UPCOMING_DEADLINE,
    HEALTH_REMINDER,
    HABIT_SUGGESTION,
    GOAL_SUGGESTION,
    CUSTOM
};

struct Recommendation {
    String id;
    unsigned long timestamp;
    RecommendationCategory category;
    String title;
    String description;
    float priority;
    bool dismissed;
    bool acted;
    unsigned long expiry;
    String explanation;
    String confidence;
    String sourceData;
    float relevanceScore;

    Recommendation() noexcept
        : timestamp(0), priority(0), dismissed(false), acted(false),
          expiry(0), confidence("medium"), relevanceScore(0.0f) {}
};

// ========================================================================
// ExecutiveAssistant
// ========================================================================

class ExecutiveAssistant {
public:
    ExecutiveAssistant() noexcept;
    ~ExecutiveAssistant() noexcept;

    ExecutiveAssistant(const ExecutiveAssistant&) = delete;
    ExecutiveAssistant& operator=(const ExecutiveAssistant&) = delete;
    ExecutiveAssistant(ExecutiveAssistant&&) = delete;
    ExecutiveAssistant& operator=(ExecutiveAssistant&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // --- Suggestion API (existing) ---
    [[nodiscard]] std::vector<Suggestion> getActiveSuggestions(uint8_t minPriority = 0) const noexcept;
    void dismissSuggestion(const String& suggestionId) noexcept;
    void snoozeSuggestion(const String& suggestionId, unsigned long durationMs) noexcept;
    void suggest(const String& title, const String& description, uint8_t priority = 1,
                 SuggestionCategory category = SuggestionCategory::CUSTOM) noexcept;
    void recordUserResponse(const String& suggestionId, bool accepted) noexcept;
    [[nodiscard]] unsigned long getAdaptiveCooldown(SuggestionCategory category) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

    // --- Recommendation API (merged from RecommendationManager) ---
    void generateAllRecommendations() noexcept;
    [[nodiscard]] String generateStudySuggestion() noexcept;
    [[nodiscard]] String generateBreakReminder() noexcept;
    [[nodiscard]] String generateProjectContinuation() noexcept;
    [[nodiscard]] String generateMemoryReview() noexcept;
    [[nodiscard]] String generateDeadlineReminder() noexcept;
    [[nodiscard]] String generateHealthReminder() noexcept;
    [[nodiscard]] String addRecommendation(RecommendationCategory cat, const String& title,
                                            const String& description, float priority = 0.5f,
                                            unsigned long expiry = 0) noexcept;
    void dismissRecommendation(const String& id) noexcept;
    void markRecommendationActed(const String& id) noexcept;
    [[nodiscard]] std::vector<Recommendation> getActiveRecommendations() const noexcept;
    [[nodiscard]] std::vector<Recommendation> getRecommendationHistory(size_t count = 20) const noexcept;
    [[nodiscard]] std::vector<Recommendation> getRecommendationsByCategory(RecommendationCategory cat) const noexcept;
    [[nodiscard]] String getRecommendationsJson(bool activeOnly = true) const noexcept;
    [[nodiscard]] String explainRecommendation(const String& recId) const noexcept;
    [[nodiscard]] std::vector<Recommendation> getRecommendationsByConfidence(const String& confidenceLevel) const noexcept;
    void setExplanationDetailLevel(uint8_t level) noexcept;
    [[nodiscard]] bool saveRecommendations() noexcept;
    [[nodiscard]] bool loadRecommendations() noexcept;

private:
    static constexpr const char* kLogCategory = "ExecutiveAssistant";
    static constexpr unsigned long kCheckIntervalMs = 60000;
    static constexpr unsigned long kSuggestionCooldownMs = 3600000;
    static constexpr size_t kMaxSuggestions = 20;

    // --- Suggestion internals ---
    void checkStudyRoutine() noexcept;
    void checkProjectUpdates() noexcept;
    void checkUpcomingReminders() noexcept;
    void checkWiFiHealth() noexcept;
    void checkStorageHealth() noexcept;
    void checkBackupReminder() noexcept;
    void checkHabitReminders() noexcept;
    void checkFocusTime() noexcept;
    void generateDailyBrief() noexcept;
    void generateProductivityInsight() noexcept;
    void pruneExpired() noexcept;
    bool isCooldownActive(SuggestionCategory category) const noexcept;

    // --- Recommendation internals ---
    static constexpr size_t kMaxActiveRecs = 10;
    static constexpr size_t kMaxHistoryRecs = 50;
    static constexpr unsigned long kRecGenIntervalMs = 3600000;

    String generateRecId() noexcept;
    void trimRecHistory() noexcept;
    void cleanExpiredRecs() noexcept;
    const char* recCategoryToString(RecommendationCategory c) const noexcept;
    RecommendationCategory recStringToCategory(const String& s) const noexcept;
    String serializeRec(const Recommendation& r) const noexcept;
    Recommendation deserializeRec(const String& json) const noexcept;

    // --- Suggestion state ---
    bool m_initialized;
    std::vector<Suggestion> m_suggestions;
    unsigned long m_lastCheckTime;
    unsigned long m_categoryCooldowns[14];
    bool m_dailyBriefShown;
    unsigned long m_lastBriefDate;
    uint8_t m_acceptedCount[14];
    uint8_t m_dismissedCount[14];
    unsigned long m_lastPatternObservation;

    // --- Recommendation state ---
    std::vector<Recommendation> m_recommendations;
    unsigned long m_lastRecIdCounter;
    unsigned long m_lastRecGenTime;
    uint8_t m_explanationDetailLevel;
    bool m_recsDirty;
};

extern ExecutiveAssistant executiveAssistant;

#endif // AURA_EXECUTIVE_ASSISTANT_H
