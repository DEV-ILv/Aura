#ifndef AURA_LEARNING_MANAGER_H
#define AURA_LEARNING_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class LearningPatternType : uint8_t {
    SCHEDULE_REPEAT,
    FREQUENT_COMMAND,
    DAILY_ROUTINE,
    STUDY_HABIT,
    SLEEP_PATTERN,
    CODING_SESSION,
    PROJECT_PREFERENCE,
    LEARNING_TOPIC,
    CUSTOM
};

struct LearningObservation {
    String id;
    unsigned long timestamp;
    String data;                // The observed data
    String category;            // Free-form category
    float frequency;            // 0-1 how often this occurs
    LearningPatternType patternType;
    
    LearningObservation() noexcept : timestamp(0), frequency(0), patternType(LearningPatternType::CUSTOM) {}
};

struct LearnedPattern {
    String id;
    String name;
    String description;
    LearningPatternType type;
    float confidence;           // 0-1
    unsigned long firstObserved;
    unsigned long lastObserved;
    size_t occurrenceCount;
    String suggestion;          // Auto-generated suggestion
    bool actionable;            // Can auto-create habit/reminder/planner
    
    LearnedPattern() noexcept : confidence(0), firstObserved(0), lastObserved(0), occurrenceCount(0), actionable(false) {}
};

class LearningManager {
public:
    LearningManager() noexcept;
    ~LearningManager() noexcept;
    
    LearningManager(const LearningManager&) = delete;
    LearningManager& operator=(const LearningManager&) = delete;
    LearningManager(LearningManager&&) = delete;
    LearningManager& operator=(LearningManager&&) = delete;
    
    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    
    // Observe API
    void observe(const String& data, const String& category = "", LearningPatternType type = LearningPatternType::CUSTOM) noexcept;
    void observeSchedule(unsigned long timestamp, const String& activity) noexcept;
    void observeCommand(const String& command) noexcept;
    void observeRoutine(const String& routineName, unsigned long timeOfDay) noexcept;
    void observeSession(const String& sessionType, unsigned long duration) noexcept;
    
    // Pattern detection
    [[nodiscard]] std::vector<LearnedPattern> detectPatterns() noexcept;
    [[nodiscard]] std::vector<LearnedPattern> getActivePatterns(size_t minConfidence = 0) const noexcept;
    [[nodiscard]] String generateSuggestion(const LearnedPattern& pattern) const noexcept;
    
    // Query
    [[nodiscard]] std::vector<LearningObservation> getRecentObservations(size_t count = 20) const noexcept;
    [[nodiscard]] std::vector<LearnedPattern> getPatternsByType(LearningPatternType type) const noexcept;
    [[nodiscard]] String getPatternsJson() const noexcept;
    
    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    
    [[nodiscard]] bool isInitialized() const noexcept;
    
private:
    static constexpr const char* kLogCategory = "LearningMgr";
    static constexpr size_t kMaxObservations = LEARNING_MAX_OBSERVATIONS;
    static constexpr size_t kMaxPatterns = LEARNING_MAX_PATTERNS;
    static constexpr float kMinConfidence = LEARNING_MIN_CONFIDENCE;
    static constexpr unsigned long kObserveIntervalMs = LEARNING_OBSERVE_INTERVAL_MS;
    
    String generateId() noexcept;
    void trimObservations() noexcept;
    void analyzePatterns() noexcept;
    const char* patternTypeToString(LearningPatternType t) const noexcept;
    LearningPatternType stringToPatternType(const String& s) const noexcept;
    
    String serializeObservation(const LearningObservation& obs) const noexcept;
    String serializePattern(const LearnedPattern& p) const noexcept;
    LearningObservation deserializeObservation(const String& json) const noexcept;
    LearnedPattern deserializePattern(const String& json) const noexcept;
    
    bool m_initialized;
    bool m_dirty;
    std::vector<LearningObservation> m_observations;
    std::vector<LearnedPattern> m_patterns;
    unsigned long m_lastIdCounter;
    unsigned long m_lastAnalysisTime;
};

extern LearningManager learningManager;

#endif
