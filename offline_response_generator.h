#ifndef AURA_OFFLINE_RESPONSE_GENERATOR_H
#define AURA_OFFLINE_RESPONSE_GENERATOR_H

#include <Arduino.h>
#include <vector>
#include "intent_classifier.h"

class OfflineResponseGenerator {
public:
    OfflineResponseGenerator() noexcept;
    ~OfflineResponseGenerator() noexcept;

    OfflineResponseGenerator(const OfflineResponseGenerator&) = delete;
    OfflineResponseGenerator& operator=(const OfflineResponseGenerator&) = delete;

    String generate(const IntentResult& intent) noexcept;

private:
    String handleGreeting() const noexcept;
    String handleSmallTalk() const noexcept;
    String handleCapabilities() const noexcept;
    String handleReminderQuery() const noexcept;
    String handleGoalQuery() const noexcept;
    String handleHabitQuery() const noexcept;
    String handlePlannerQuery() const noexcept;
    String handleMemoryQuery(const IntentResult& intent) const noexcept;
    String handleKnowledgeQuery(const IntentResult& intent) const noexcept;
    String handleSettingsQuery() const noexcept;
    String handleWifiStatus() const noexcept;
    String handleStorageStatus() const noexcept;
    String handlePersonalityQuery() const noexcept;
    String handleDecisionQuery() const noexcept;
    String handleLearningQuery() const noexcept;
    String handleRecommendationQuery() const noexcept;
    String handlePredictionQuery() const noexcept;
    String handleDocumentQuery() const noexcept;
    String handleWorkspaceQuery() const noexcept;
    String handleVaultQuery() const noexcept;
    String handleDeveloperQuery() const noexcept;
    String handleStudyQuery() const noexcept;
    String handleFlashcardQuery() const noexcept;
    String handleQuizQuery() const noexcept;
    String handlePairQuery() const noexcept;
    String handleSyncQuery() const noexcept;
    String handleDashboardQuery() const noexcept;
    String handleCreateSkillQuery() const noexcept;
    String handleUnknown() const noexcept;

    String joinStrings(const std::vector<String>& items, const String& separator) const noexcept;

    static constexpr const char* kLogCategory = "OfflineResponse";
};

#endif
