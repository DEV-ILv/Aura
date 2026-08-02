#include "conversation_context_engine.h"

#include <time.h>
#include "logger.h"

ConversationContextEngine conversationContext;

namespace {
constexpr size_t kMaxUserTextLen = 64;
constexpr size_t kMaxResponseLen = 160;

String truncate(const String& s, size_t maxLen) noexcept {
    if (s.length() <= maxLen) return s;
    return s.substring(0, maxLen);
}
}  // namespace

ConversationContextEngine::ConversationContextEngine() noexcept
    : m_turnCount(0),
      m_previousIntent(IntentType::UNKNOWN) {}

ConversationContextEngine::~ConversationContextEngine() noexcept {}

void ConversationContextEngine::reset() noexcept {
    for (size_t i = 0; i < kMaxTurns; ++i) {
        m_turns[i] = ContextTurn();
    }
    m_turnCount = 0;
    m_currentTopic = "";
    m_previousTopic = "";
    m_previousIntent = IntentType::UNKNOWN;
    m_previousResponse = "";
    m_project = "";
    m_task = "";
    m_activity = "";
    m_mood = "";
    m_workspace = "";
    m_activeGoal = "";
    m_upcomingReminder = "";
    m_studySession = "";
    m_recentRecommendation = "";
}

void ConversationContextEngine::recordTurn(const IntentResult& intent,
                                           const String& userText,
                                           const String& response) noexcept {
    m_previousTopic = m_currentTopic;
    m_previousIntent = intent.type;
    m_previousResponse = truncate(response, kMaxResponseLen);

    if (intent.entityCount > 0 && !intent.entities[0].isEmpty()) {
        m_currentTopic = intent.entities[0];
    } else if (!userText.isEmpty()) {
        m_currentTopic = truncate(userText, 32);
    }

    ContextTurn turn;
    turn.intent = intent.type;
    turn.topic = m_currentTopic;
    turn.userText = truncate(userText, kMaxUserTextLen);
    turn.response = m_previousResponse;
    m_turns[m_turnCount % kMaxTurns] = turn;
    m_turnCount++;
}

void ConversationContextEngine::setTopic(const String& topic) noexcept {
    m_previousTopic = m_currentTopic;
    m_currentTopic = topic;
}

void ConversationContextEngine::setProject(const String& project) noexcept {
    m_project = project;
}

void ConversationContextEngine::setTask(const String& task) noexcept {
    m_task = task;
}

void ConversationContextEngine::setActivity(const String& activity) noexcept {
    m_activity = activity;
}

void ConversationContextEngine::setMood(const String& mood) noexcept {
    m_mood = mood;
}

void ConversationContextEngine::setWorkspace(const String& workspace) noexcept {
    m_workspace = workspace;
}

void ConversationContextEngine::setActiveGoal(const String& goal) noexcept {
    m_activeGoal = goal;
}

void ConversationContextEngine::setUpcomingReminder(const String& reminder) noexcept {
    m_upcomingReminder = reminder;
}

void ConversationContextEngine::setStudySession(const String& session) noexcept {
    m_studySession = session;
}

void ConversationContextEngine::setRecentRecommendation(const String& recommendation) noexcept {
    m_recentRecommendation = recommendation;
}

String ConversationContextEngine::currentTopic() const noexcept {
    return m_currentTopic;
}

String ConversationContextEngine::previousTopic() const noexcept {
    return m_previousTopic;
}

IntentType ConversationContextEngine::previousIntent() const noexcept {
    return m_previousIntent;
}

String ConversationContextEngine::previousResponse() const noexcept {
    return m_previousResponse;
}

String ConversationContextEngine::lastUserText() const noexcept {
    if (m_turnCount == 0) return "";
    return m_turns[(m_turnCount - 1) % kMaxTurns].userText;
}

String ConversationContextEngine::project() const noexcept { return m_project; }
String ConversationContextEngine::task() const noexcept { return m_task; }
String ConversationContextEngine::activity() const noexcept { return m_activity; }
String ConversationContextEngine::mood() const noexcept { return m_mood; }
String ConversationContextEngine::workspace() const noexcept { return m_workspace; }
String ConversationContextEngine::activeGoal() const noexcept { return m_activeGoal; }
String ConversationContextEngine::upcomingReminder() const noexcept { return m_upcomingReminder; }
String ConversationContextEngine::studySession() const noexcept { return m_studySession; }
String ConversationContextEngine::recentRecommendation() const noexcept { return m_recentRecommendation; }

size_t ConversationContextEngine::turnCount() const noexcept {
    return m_turnCount;
}

bool ConversationContextEngine::isContinuation() const noexcept {
    if (m_previousTopic.isEmpty() || m_currentTopic.isEmpty()) return false;
    return m_previousTopic.equalsIgnoreCase(m_currentTopic);
}

bool ConversationContextEngine::hasTopic() const noexcept {
    return !m_currentTopic.isEmpty();
}

bool ConversationContextEngine::isTimeKnown() const noexcept {
    time_t now = time(nullptr);
    return now > 100000;  // RTC synced (NTP / set manually)
}

TimePeriod ConversationContextEngine::timePeriod() const noexcept {
    if (!isTimeKnown()) return TimePeriod::UNKNOWN;
    struct tm t;
    if (!getLocalTime(&t)) return TimePeriod::UNKNOWN;
    const uint8_t hour = static_cast<uint8_t>(t.tm_hour);
    if (hour >= 5 && hour < 12)  return TimePeriod::MORNING;
    if (hour >= 12 && hour < 17) return TimePeriod::AFTERNOON;
    if (hour >= 17 && hour < 21) return TimePeriod::EVENING;
    return TimePeriod::NIGHT;
}

bool ConversationContextEngine::isWeekend() const noexcept {
    if (!isTimeKnown()) return false;
    struct tm t;
    if (!getLocalTime(&t)) return false;
    return (t.tm_wday == 0 || t.tm_wday == 6);
}

String ConversationContextEngine::timeGreeting() const noexcept {
    switch (timePeriod()) {
        case TimePeriod::MORNING:   return "Good morning";
        case TimePeriod::AFTERNOON: return "Good afternoon";
        case TimePeriod::EVENING:   return "Good evening";
        case TimePeriod::NIGHT:     return "Good evening";
        default:                    return "";
    }
}

String ConversationContextEngine::timeClause() const noexcept {
    switch (timePeriod()) {
        case TimePeriod::MORNING:   return "It's still morning";
        case TimePeriod::AFTERNOON: return "It's the middle of the day";
        case TimePeriod::EVENING:   return "It's evening now";
        case TimePeriod::NIGHT:     return "It's getting late";
        default:                    return "";
    }
}
