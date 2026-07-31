#ifndef AURA_INTENT_CLASSIFIER_H
#define AURA_INTENT_CLASSIFIER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

enum class IntentType : uint8_t {
    GREETING,
    SMALL_TALK,
    CAPABILITIES,
    REMINDER_QUERY,
    GOAL_QUERY,
    HABIT_QUERY,
    PLANNER_QUERY,
    MEMORY_QUERY,
    KNOWLEDGE_QUERY,
    SETTINGS_QUERY,
    WIFI_STATUS,
    STORAGE_STATUS,
    PERSONALITY_QUERY,
    DECISION_QUERY,
    LEARNING_QUERY,
    RECOMMENDATION_QUERY,
    PREDICTION_QUERY,
    DOCUMENT_QUERY,
    WORKSPACE_QUERY,
    DEVELOPER_QUERY,
    INTENT_STUDY,
    INTENT_FLASHCARD,
    INTENT_QUIZ,
    INTENT_PAIR,
    INTENT_SYNC,
    INTENT_DASHBOARD,
    INTENT_CREATE_SKILL,
    UNKNOWN
};

struct IntentResult {
    IntentType type;
    float confidence;
    String entities[4];
    size_t entityCount;

    IntentResult() noexcept
        : type(IntentType::UNKNOWN), confidence(0.0f), entityCount(0) {}
};

class IntentClassifier {
public:
    IntentClassifier() noexcept;
    ~IntentClassifier() noexcept;

    IntentClassifier(const IntentClassifier&) = delete;
    IntentClassifier& operator=(const IntentClassifier&) = delete;

    IntentResult classify(const String& text) const noexcept;

private:
    struct IntentPattern {
        IntentType type;
        const char* const* keywords;
        size_t keywordCount;
    };

    static const IntentPattern kPatterns[];
    static constexpr size_t kPatternCount = 28;

    float matchPattern(const String& text, const IntentPattern& pattern) const noexcept;
    void extractEntities(const String& text, IntentResult& result) const noexcept;
    bool containsKeyword(const String& text, const char* keyword) const noexcept;

    static constexpr const char* kLogCategory = "IntentClassifier";
};

#endif
