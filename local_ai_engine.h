#ifndef AURA_LOCAL_AI_ENGINE_H
#define AURA_LOCAL_AI_ENGINE_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include "config.h"
#include "intent_classifier.h"

/**
 * @class LocalAIEngine
 * @brief Phase-3 Local AI Engine V2 coordinator.
 *
 * Implements the offline intelligence pipeline:
 *
 *   Intent -> Context Engine -> Memory Retrieval -> Knowledge Retrieval
 *         -> Planner -> Goals -> Recommendation Engine -> Personality
 *         -> Sentence Generation -> Offline Response
 *
 * It composes data-driven responses from the existing managers (never
 * replacing them), injects relevant memories and knowledge, adds natural
 * recommendations and follow-up questions, respects time of day, honours the
 * active personality register, and guarantees response variation through the
 * SentenceGenerationEngine fragment pools.
 *
 * The class is a drop-in upgrade: OfflineResponseGenerator delegates to it,
 * so the existing Intent Classifier -> OfflineResponseGenerator -> managers
 * architecture and all public APIs remain unchanged.
 */
class LocalAIEngine {
public:
    LocalAIEngine() noexcept;
    ~LocalAIEngine() noexcept;

    LocalAIEngine(const LocalAIEngine&) = delete;
    LocalAIEngine& operator=(const LocalAIEngine&) = delete;

    /**
     * @brief Generate an offline response for a classified intent.
     * @param intent Classified intent (entities[0] = topic when present).
     * @return Composed, personality-styled, context-aware response text.
     */
    String generate(const IntentResult& intent) noexcept;

    /**
     * @brief Generate a response with the original raw user text (used for
     *        topic tracking and exact-question cache hits).
     */
    String generate(const IntentResult& intent, const String& rawText) noexcept;

    /** @brief Run the offline self-test suite; returns human-readable report. */
    String runSelfTest() noexcept;

    /** @brief Status JSON for the web portal / REST surface. */
    String getStatusJSON() const noexcept;

    /** @brief Reset all rolling state (context, cache). */
    void reset() noexcept;

    // Statistics
    size_t getGeneratedCount() const noexcept;
    unsigned long getLastLatencyMs() const noexcept;
    size_t getCacheSize() const noexcept;
    size_t getCacheHits() const noexcept;

private:
    // --- Pipeline stages ---
    void syncContextFromManagers() noexcept;
    String composeResponse(IntentType type, const String& rawText) noexcept;

    // --- Per-intent data composition (uses managers only as data sources) ---
    String composeGreeting(const String& rawText) noexcept;
    String composeSmallTalk(const String& rawText) noexcept;
    String composeCapabilities() noexcept;
    String composeReminders() noexcept;
    String composeGoals() noexcept;
    String composeHabits() noexcept;
    String composePlanner() noexcept;
    String composeMemory(const IntentResult& intent) noexcept;
    String composeKnowledge(const IntentResult& intent) noexcept;
    String composeSettings() noexcept;
    String composeWifi() noexcept;
    String composeStorage() noexcept;
    String composePersonality() noexcept;
    String composeDecisions() noexcept;
    String composeLearning() noexcept;
    String composeRecommendations() noexcept;
    String composePredictions() noexcept;
    String composeDocuments() noexcept;
    String composeWorkspaces() noexcept;
    String composeDeveloper() noexcept;
    String composeStudy() noexcept;
    String composeFlashcards() noexcept;
    String composeQuiz() noexcept;
    String composePair() noexcept;
    String composeSync() noexcept;
    String composeDashboard() noexcept;
    String composeCreateSkill() noexcept;
    String composeUnknown(const String& rawText) noexcept;

    // --- Enrichment helpers ---
    String retrieveMemoryClause(const String& topic) noexcept;
    String retrieveKnowledgeClause(const String& topic) noexcept;
    String recommendationClause() noexcept;
    String followUpClause(IntentType type) noexcept;
    String timePrefix() noexcept;

    /** @brief Wrap a data body: time prefix, closing, follow-up, variation. */
    String finalize(IntentType type, String body, bool addClosing = false,
                    bool addFollowUp = true) noexcept;

    // Internal state
    bool m_initialized;
    size_t m_generatedCount;
    unsigned long m_lastLatencyMs;
    unsigned long m_lastSelfTestDate;
};

extern LocalAIEngine localAIEngine;

#endif // AURA_LOCAL_AI_ENGINE_H
