#ifndef AURA_CONVERSATION_CONTEXT_ENGINE_H
#define AURA_CONVERSATION_CONTEXT_ENGINE_H

#include <Arduino.h>
#include <cstdint>
#include "config.h"
#include "intent_classifier.h"

/**
 * @enum TimePeriod
 * @brief Coarse time-of-day buckets used for time-aware responses.
 */
enum class TimePeriod : uint8_t {
    MORNING,
    AFTERNOON,
    EVENING,
    NIGHT,
    UNKNOWN
};

/**
 * @struct ContextTurn
 * @brief One recorded turn of the conversation (bounded, compact).
 */
struct ContextTurn {
    IntentType intent;
    String topic;       ///< Primary entity / subject of the turn
    String userText;    ///< Truncated raw user input
    String response;    ///< Truncated assistant response

    ContextTurn() noexcept
        : intent(IntentType::UNKNOWN) {}
};

/**
 * @class ConversationContextEngine
 * @brief Maintains the rolling conversational state for the Local AI Engine.
 *
 * Tracks current/previous topic, conversation history, previous response,
 * previous intent, and ambient user state (project, task, activity, mood,
 * workspace, active goal, upcoming reminder, study session, recent
 * recommendations) so responses can reference context naturally.
 */
class ConversationContextEngine {
public:
    static constexpr size_t kMaxTurns = LOCAL_AI_HISTORY_TURNS;

    ConversationContextEngine() noexcept;
    ~ConversationContextEngine() noexcept;

    ConversationContextEngine(const ConversationContextEngine&) = delete;
    ConversationContextEngine& operator=(const ConversationContextEngine&) = delete;

    /** @brief Clear all state (e.g. after factory reset). */
    void reset() noexcept;

    /** @brief Record a completed exchange into the rolling history. */
    void recordTurn(const IntentResult& intent, const String& userText,
                    const String& response) noexcept;

    // --- User state setters (fed by managers / pipeline) ---
    void setTopic(const String& topic) noexcept;
    void setProject(const String& project) noexcept;
    void setTask(const String& task) noexcept;
    void setActivity(const String& activity) noexcept;
    void setMood(const String& mood) noexcept;
    void setWorkspace(const String& workspace) noexcept;
    void setActiveGoal(const String& goal) noexcept;
    void setUpcomingReminder(const String& reminder) noexcept;
    void setStudySession(const String& session) noexcept;
    void setRecentRecommendation(const String& recommendation) noexcept;

    // --- Getters ---
    String currentTopic() const noexcept;
    String previousTopic() const noexcept;
    IntentType previousIntent() const noexcept;
    String previousResponse() const noexcept;
    String lastUserText() const noexcept;
    String project() const noexcept;
    String task() const noexcept;
    String activity() const noexcept;
    String mood() const noexcept;
    String workspace() const noexcept;
    String activeGoal() const noexcept;
    String upcomingReminder() const noexcept;
    String studySession() const noexcept;
    String recentRecommendation() const noexcept;
    size_t turnCount() const noexcept;

    /** @brief True when the last two turns shared the same topic. */
    bool isContinuation() const noexcept;

    /** @brief True when a concrete topic is recorded. */
    bool hasTopic() const noexcept;

    // --- Time awareness ---
    TimePeriod timePeriod() const noexcept;
    bool isWeekend() const noexcept;
    bool isTimeKnown() const noexcept;

    /** @brief Natural time greeting base, e.g. "Good evening". Empty when unknown. */
    String timeGreeting() const noexcept;

    /** @brief Ambient time clause, e.g. "It's getting late". Empty when unknown. */
    String timeClause() const noexcept;

private:
    ContextTurn m_turns[kMaxTurns];
    size_t m_turnCount;
    String m_currentTopic;
    String m_previousTopic;
    IntentType m_previousIntent;
    String m_previousResponse;
    String m_project;
    String m_task;
    String m_activity;
    String m_mood;
    String m_workspace;
    String m_activeGoal;
    String m_upcomingReminder;
    String m_studySession;
    String m_recentRecommendation;
};

extern ConversationContextEngine conversationContext;

#endif // AURA_CONVERSATION_CONTEXT_ENGINE_H
