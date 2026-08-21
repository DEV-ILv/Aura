#include "local_ai_engine.h"

#include <WiFi.h>
#include "logger.h"
#include "memory_manager.h"
#include "knowledge_graph_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "planner_manager.h"
#include "reminder_manager.h"
#include "settings_manager.h"
#include "storage_manager.h"
#include "personality_manager.h"
#include "context_manager.h"
#include "decision_manager.h"
#include "learning_manager.h"
#include "executive_assistant.h"
#include "prediction_manager.h"
#include "document_manager.h"
#include "workspace_manager.h"
#include "performance_manager.h"
#include "study_manager.h"
#include "skill_manager.h"
#include "companion_manager.h"
#include "vault_manager.h"
#include "conversation_context_engine.h"
#include "personality_engine.h"
#include "recommendation_engine.h"
#include "local_ai_cache.h"

LocalAIEngine localAIEngine;

namespace {

constexpr const char* kLogCategory = "LocalAI";

// Follow-up question pools per intent family (flash-resident).
constexpr const char* kFollowUpGreeting[] = {
    "What would you like to start with?",
    "Shall I read you today's schedule?",
    "Would you like a quick look at your goals?"
};
constexpr const char* kFollowUpReminder[] = {
    "Would you like me to walk you through each one?",
    "Should I set a new reminder for you?",
    "Want me to read today's schedule?"
};
constexpr const char* kFollowUpPlanner[] = {
    "Would you like me to suggest your next action?",
    "Shall I break that into smaller steps?",
    "Want to mark one as done?"
};
constexpr const char* kFollowUpGoal[] = {
    "Would you like to focus on one of these goals?",
    "Shall I break that into milestones?",
    "Want a plan to reach the top goal?"
};
constexpr const char* kFollowUpHabit[] = {
    "Would you like me to remind you later?",
    "Shall I help you complete one now?",
    "Want me to track that for you?"
};
constexpr const char* kFollowUpMemory[] = {
    "Would you like me to recall more about that?",
    "Shall I save that as a new memory?",
    "Want me to link it to a project?"
};
constexpr const char* kFollowUpKnowledge[] = {
    "Would you like to know more?",
    "Shall I search my knowledge graph further?",
    "Want me to connect that to a goal?"
};
constexpr const char* kFollowUpGeneric[] = {
    "Anything else you'd like to know?",
    "Would you like to go deeper on that?",
    "What else can I help with?"
};
constexpr const char* kFollowUpStudy[] = {
    "Would you like to start a study session?",
    "Shall I quiz you on this subject?",
    "Want me to show you your flashcards?"
};
constexpr const char* kFollowUpWorkspace[] = {
    "Would you like to open one of these?",
    "Shall I add a project to a workspace?",
    "Want me to switch the active workspace?"
};

const char* pickFollowUp(const char* const* pool, size_t count) noexcept {
    if (count == 0) return "";
    return pool[static_cast<size_t>(random(static_cast<long>(count)))];
}

}  // namespace

LocalAIEngine::LocalAIEngine() noexcept
    : m_initialized(true), m_generatedCount(0), m_lastLatencyMs(0), m_lastSelfTestDate(0) {}

LocalAIEngine::~LocalAIEngine() noexcept {}

void LocalAIEngine::reset() noexcept {
    conversationContext.reset();
    localAICache.reset();
    m_generatedCount = 0;
    m_lastLatencyMs = 0;
}

String LocalAIEngine::generate(const IntentResult& intent) noexcept {
    return generate(intent, "");
}

String LocalAIEngine::generate(const IntentResult& intent, const String& rawText) noexcept {
    const unsigned long start = millis();
    personalityEngine.refresh();
    syncContextFromManagers();

    // Exact-repeat cache fast path.
    if (!rawText.isEmpty()) {
        String cached;
        if (localAICache.lookup(rawText, cached)) {
            m_lastLatencyMs = millis() - start;
            return cached;
        }
    }

    String response = composeResponse(intent.type, rawText);
    if (response.isEmpty()) {
        response = composeUnknown(rawText);
    }

    // Bound the response length.
    if (response.length() > static_cast<int>(TINYAI_MAX_RESPONSE_LEN)) {
        response = response.substring(0, TINYAI_MAX_RESPONSE_LEN);
    }

    m_lastLatencyMs = millis() - start;
    m_generatedCount++;

    conversationContext.recordTurn(intent, rawText, response);
    localAICache.noteResponse(response);
    if (!rawText.isEmpty()) {
        localAICache.remember(rawText, response);
    }

    LOG_INFO(kLogCategory, "Intent=%d ctx=%d ms=%lu cache=%u resp=%s",
             static_cast<int>(intent.type), static_cast<int>(conversationContext.turnCount()),
             static_cast<unsigned long>(m_lastLatencyMs),
             static_cast<unsigned int>(response.length()), response.c_str());
    return response;
}

// ============================================================================
// Context synchronisation — pulls live state from the existing managers
// ============================================================================

void LocalAIEngine::syncContextFromManagers() noexcept {
    if (contextManager.isInitialized()) {
        const SystemContext& ctx = contextManager.getContext();
        if (!ctx.currentProject.isEmpty()) conversationContext.setProject(ctx.currentProject);
        if (!ctx.currentTask.isEmpty()) conversationContext.setTask(ctx.currentTask);
        if (!ctx.currentActivity.isEmpty()) conversationContext.setActivity(ctx.currentActivity);
        if (!ctx.currentMood.isEmpty()) conversationContext.setMood(ctx.currentMood);
    }
    if (workspaceManager.isInitialized()) {
        const Workspace* ws = workspaceManager.getActiveWorkspace();
        if (ws != nullptr && !ws->name.isEmpty()) conversationContext.setWorkspace(ws->name);
    }
    if (goalManager.isInitialized()) {
        auto goals = goalManager.getActiveGoals();
        if (!goals.empty()) conversationContext.setActiveGoal(goals[0].title);
    }
    if (reminderManager.isInitialized()) {
        std::vector<Reminder> reminders;
        size_t count = reminderManager.getReminders(reminders);
        if (count > 0) conversationContext.setUpcomingReminder(reminders[0].title);
    }
    if (studyManager.isInitialized()) {
        auto due = studyManager.getDueSubjects();
        if (!due.empty()) conversationContext.setStudySession(due[0].name);
    }
    if (executiveAssistant.isInitialized()) {
        auto recs = executiveAssistant.getActiveRecommendations();
        if (!recs.empty()) conversationContext.setRecentRecommendation(recs[0].title);
    }
}

// ============================================================================
// Pipeline dispatch
// ============================================================================

String LocalAIEngine::composeResponse(IntentType type, const String& rawText) noexcept {
    switch (type) {
        case IntentType::GREETING:            return composeGreeting(rawText);
        case IntentType::SMALL_TALK:          return composeSmallTalk(rawText);
        case IntentType::CAPABILITIES:        return composeCapabilities();
        case IntentType::REMINDER_QUERY:      return composeReminders();
        case IntentType::GOAL_QUERY:          return composeGoals();
        case IntentType::HABIT_QUERY:         return composeHabits();
        case IntentType::PLANNER_QUERY:       return composePlanner();
        case IntentType::MEMORY_QUERY:        return composeMemory(IntentResult());
        case IntentType::KNOWLEDGE_QUERY:     return composeKnowledge(IntentResult());
        case IntentType::SETTINGS_QUERY:      return composeSettings();
        case IntentType::WIFI_STATUS:         return composeWifi();
        case IntentType::STORAGE_STATUS:      return composeStorage();
        case IntentType::PERSONALITY_QUERY:   return composePersonality();
        case IntentType::DECISION_QUERY:      return composeDecisions();
        case IntentType::LEARNING_QUERY:      return composeLearning();
        case IntentType::RECOMMENDATION_QUERY: return composeRecommendations();
        case IntentType::PREDICTION_QUERY:    return composePredictions();
        case IntentType::DOCUMENT_QUERY:      return composeDocuments();
        case IntentType::WORKSPACE_QUERY:     return composeWorkspaces();
        case IntentType::DEVELOPER_QUERY:     return composeDeveloper();
        case IntentType::INTENT_STUDY:        return composeStudy();
        case IntentType::INTENT_FLASHCARD:    return composeFlashcards();
        case IntentType::INTENT_QUIZ:         return composeQuiz();
        case IntentType::INTENT_PAIR:         return composePair();
        case IntentType::INTENT_SYNC:         return composeSync();
        case IntentType::INTENT_DASHBOARD:    return composeDashboard();
        case IntentType::INTENT_CREATE_SKILL: return composeCreateSkill();
        case IntentType::UNKNOWN:             return composeUnknown(rawText);
        default:                              return composeUnknown(rawText);
    }
}

// ============================================================================
// Greeting / small talk / capabilities / unknown
// ============================================================================

String LocalAIEngine::timePrefix() noexcept {
    const String greeting = conversationContext.timeGreeting();
    if (greeting.isEmpty()) return "";
    return greeting + ". ";
}

String LocalAIEngine::composeGreeting(const String& rawText) noexcept {
    String greeting = timePrefix();
    greeting += sentenceEngine.pickGreeting(personalityEngine.knobs().style);
    greeting += ".";

    // Context awareness.
    if (conversationContext.hasTopic()) {
        greeting += " I remember we were talking about ";
        greeting += conversationContext.currentTopic();
        greeting += ".";
    }
    if (!conversationContext.workspace().isEmpty()) {
        greeting += String(" ") + sentenceEngine.pickTransition(personalityEngine.knobs().style) + ", you're working in the \"";
        greeting += conversationContext.workspace();
        greeting += "\" workspace.";
    }

    const bool minimal = personalityEngine.knobs().verbosity == 0;
    return finalize(IntentType::GREETING, greeting, !minimal, !minimal);
}

String LocalAIEngine::composeSmallTalk(const String& rawText) noexcept {
    String body;
    const String clause = conversationContext.timeClause();
    if (!clause.isEmpty()) {
        body = sentenceEngine.capitalise(clause) + ".";
    }
    if (!conversationContext.mood().isEmpty()) {
        if (!body.isEmpty()) body += " ";
        body += "You seem " + conversationContext.mood() + " - ";
        body += sentenceEngine.pickConfidence(0.7f);
        body += " I'm picking that up.";
    }
    body += " I'm running smoothly on my local engine";
    body += conversationContext.isTimeKnown() ? " and keeping an eye on your day" : "";
    body += ".";
    return finalize(IntentType::SMALL_TALK, body, true, true);
}

String LocalAIEngine::composeCapabilities() noexcept {
    String body = "Offline I can cover: reminders, goals, habits, planner and schedule, "
                  "memories, my knowledge graph, study subjects and flashcards, workspaces, "
                  "decisions, recommendations, predictions, documents, WiFi and storage status, "
                  "and your settings. ";
    body += String(sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
            ", when connected I can also reach Gemini for deeper questions.";
    return finalize(IntentType::CAPABILITIES, body, false, true);
}

String LocalAIEngine::composeUnknown(const String& rawText) noexcept {
    String body = "I'm not quite sure I understood that";
    if (!conversationContext.hasTopic()) {
        body += " - but I can help with reminders, goals, habits, your schedule, memories, "
                "study, and more. Try \"what can you do?\"";
    } else {
        body += ". We were discussing \"" + conversationContext.currentTopic() +
                "\" - would you like to continue there?";
    }
    body += ".";
    return finalize(IntentType::UNKNOWN, body, false, false);
}

// ============================================================================
// Data queries — composed from live manager state
// ============================================================================

String LocalAIEngine::composeReminders() noexcept {
    if (!reminderManager.isInitialized()) return "Reminder system isn't available right now.";

    std::vector<Reminder> reminders;
    size_t count = reminderManager.getReminders(reminders);
    if (count == 0) {
        String body = String("You have no reminders set") +
                      sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";
        return finalize(IntentType::REMINDER_QUERY, body, false, true);
    }

    String body = "You " + personalityEngine.possessiveVerb() + " " +
                  sentenceEngine.countPhrase(count, "reminder") + " " +
                  sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";

    // Quote a few titles (Smart Response Composition: reminders + context).
    std::vector<String> titles;
    for (size_t i = 0; i < count && i < LOCAL_AI_MAX_DATA_ITEMS; ++i) {
        titles.push_back(reminders[i].title);
    }
    body += " " + sentenceEngine.capitalise(sentenceEngine.listItems(titles, 3)) + ".";

    // Tie into planner if there are overlapping items.
    if (plannerManager.isInitialized() && !plannerManager.getTodaysTasks().empty()) {
        const String transition = sentenceEngine.pickTransition(personalityEngine.knobs().style);
        body += " " + sentenceEngine.capitalise(transition) +
                ", some of these line up with today's planner tasks.";
    }

    // Memory injection: related recollection.
    String mem = retrieveMemoryClause("reminder");
    if (!mem.isEmpty()) body += " " + mem;

    return finalize(IntentType::REMINDER_QUERY, body, false, true);
}

String LocalAIEngine::composeGoals() noexcept {
    if (!goalManager.isInitialized()) return "Goal system isn't available right now.";

    auto goals = goalManager.getActiveGoals();
    if (goals.empty()) {
        String body = String("You have no active goals") +
                      sentenceEngine.pickEnding(personalityEngine.knobs().style) + ". ";
        body += "Setting one would help me keep you on track.";
        return finalize(IntentType::GOAL_QUERY, body, false, true);
    }

    String body = "You " + personalityEngine.possessiveVerb() + " " +
                  sentenceEngine.countPhrase(goals.size(), "active goal") + ": ";
    std::vector<String> names;
    for (const auto& g : goals) {
        String item = g.title;
        if (g.progress > 0) item += " (" + String(static_cast<unsigned int>(g.progress)) + "%)";
        names.push_back(item);
    }
    body += sentenceEngine.listItems(names, LOCAL_AI_MAX_DATA_ITEMS) + ".";

    // Highlight the closest-to-complete goal as a recommendation hook.
    if (!conversationContext.activeGoal().isEmpty()) {
        body += " " + sentenceEngine.capitalise(
                            sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
                ", \"" + conversationContext.activeGoal() + "\" is your current focus.";
    }

    String knowledge = retrieveKnowledgeClause("goal");
    if (!knowledge.isEmpty()) body += " " + knowledge;

    return finalize(IntentType::GOAL_QUERY, body, false, true);
}

String LocalAIEngine::composeHabits() noexcept {
    if (!habitManager.isInitialized()) return "Habit system isn't available right now.";

    auto habits = habitManager.getAllHabits();
    if (habits.empty()) {
        String body = "You have no habits tracked yet. Building one small daily habit "
                      "would be a solid start.";
        return finalize(IntentType::HABIT_QUERY, body, false, true);
    }

    auto due = habitManager.getDueHabits();
    String body = "You're tracking " + sentenceEngine.countPhrase(habits.size(), "habit") + ". ";
    if (!due.empty()) {
        std::vector<String> dueNames;
        for (const auto& h : due) dueNames.push_back(h.name);
        body += sentenceEngine.capitalise(sentenceEngine.countPhrase(due.size(), "of them")) +
                " are due today: " + sentenceEngine.listItems(dueNames, 3) + ".";
    } else {
        body += String("Nothing is due ") + sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";
    }

    // Streak insight.
    uint16_t bestStreak = 0;
    for (const auto& h : habits) {
        if (h.streak > bestStreak) bestStreak = h.streak;
    }
    if (bestStreak > 0) {
        body += " Your best active streak is " + sentenceEngine.numberWord(bestStreak) + " day" +
                (bestStreak == 1 ? "" : "s") + ".";
    }

    return finalize(IntentType::HABIT_QUERY, body, false, true);
}

String LocalAIEngine::composePlanner() noexcept {
    if (!plannerManager.isInitialized()) return "Planner isn't available right now.";

    auto todays = plannerManager.getTodaysTasks();
    if (todays.empty()) todays = plannerManager.getUpcomingTasks();
    if (todays.empty()) {
        String body = String("You have no tasks scheduled") +
                      sentenceEngine.pickEnding(personalityEngine.knobs().style) +
                      ". Want to add one?";
        return finalize(IntentType::PLANNER_QUERY, body, false, true);
    }

    String body = "You " + personalityEngine.possessiveVerb() + " " +
                  sentenceEngine.countPhrase(todays.size(), "task") + " on your planner: ";
    std::vector<String> names;
    for (const auto& t : todays) {
        String item = t.title;
        if (t.priority > 0) item += " (priority " + String(static_cast<unsigned int>(t.priority)) + ")";
        names.push_back(item);
    }
    body += sentenceEngine.listItems(names, LOCAL_AI_MAX_DATA_ITEMS) + ".";

    // Use the planner's own next-action suggestion (Recommendation Engine hook).
    String nextAction = plannerManager.suggestNextAction();
    if (!nextAction.isEmpty() && nextAction.length() < 120) {
        body += " " + sentenceEngine.capitalise(
                            sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
                ", " + nextAction;
    }

    // Weekend / time awareness.
    if (conversationContext.isWeekend()) {
        body += " It's the weekend, so keep the load light.";
    }

    return finalize(IntentType::PLANNER_QUERY, body, false, true);
}

String LocalAIEngine::composeMemory(const IntentResult& intent) noexcept {
    if (!memoryManager.isInitialized()) return "Memory system isn't available right now.";

    String topic = conversationContext.currentTopic();
    if (topic.isEmpty()) topic = conversationContext.workspace();
    if (topic.isEmpty()) topic = conversationContext.project();

    if (topic.isEmpty()) {
        auto ranked = memoryManager.getRankedMemories(LOCAL_AI_RETRIEVAL_MEMORIES);
        if (ranked.empty()) {
            return "I don't have any stored memories yet. Tell me something and I'll remember it.";
        }
        std::vector<String> lines;
        for (const auto& m : ranked) {
            lines.push_back(m.key + ": " + m.value);
        }
        String body = "My top memories are: " + sentenceEngine.listItems(lines, 3) + ".";
        return finalize(IntentType::MEMORY_QUERY, body, false, true);
    }

    auto results = memoryManager.semanticSearch(topic, LOCAL_AI_RETRIEVAL_MEMORIES);
    if (results.empty()) results = memoryManager.search(topic, false);

    String body = "About \"" + topic + "\": ";
    if (!results.empty()) {
        std::vector<String> lines;
        for (size_t i = 0; i < results.size() && i < 3; ++i) {
            lines.push_back(results[i].key + " - " + results[i].value);
        }
        body += sentenceEngine.listItems(lines, 3) + ".";
    } else {
        auto convos = memoryManager.searchConversations(topic);
        if (!convos.empty()) {
            body += "from a past conversation: " + convos[0].summary + ".";
        } else {
            body += String("I couldn't find anything specific in my memory") +
                    sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";
        }
    }

    // Memory injection: connect to the workspace if relevant.
    if (!conversationContext.workspace().isEmpty() && !results.empty()) {
        body += " " + sentenceEngine.capitalise(
                            sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
                ", this connects to your \"" + conversationContext.workspace() + "\" workspace.";
    }

    return finalize(IntentType::MEMORY_QUERY, body, false, true);
}

String LocalAIEngine::composeKnowledge(const IntentResult& intent) noexcept {
    if (!knowledgeGraphManager.isInitialized()) {
        return "Knowledge graph isn't available right now.";
    }

    String topic = conversationContext.currentTopic();
    if (topic.isEmpty()) {
        const size_t nodes = knowledgeGraphManager.nodeCount();
        const size_t edges = knowledgeGraphManager.edgeCount();
        String body = "My knowledge graph holds " + sentenceEngine.countPhrase(nodes, "node") +
                      " and " + sentenceEngine.countPhrase(edges, "connection") + ".";
        return finalize(IntentType::KNOWLEDGE_QUERY, body, false, true);
    }

    auto nodes = knowledgeGraphManager.searchNodes(topic);
    String body = "About \"" + topic + "\": ";
    if (!nodes.empty()) {
        std::vector<String> lines;
        for (size_t i = 0; i < nodes.size() && i < 3; ++i) {
            String info = nodes[i].name;
            if (!nodes[i].value.isEmpty()) info += " - " + nodes[i].value;
            lines.push_back(info);
        }
        body += sentenceEngine.listItems(lines, 3) + ".";
    } else {
        body += String("I don't have a knowledge entry on that") +
                sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";
    }

    // Knowledge retrieval injection: related nodes via traversal.
    String related = retrieveKnowledgeClause(topic);
    if (!related.isEmpty()) body += " " + related;

    return finalize(IntentType::KNOWLEDGE_QUERY, body, false, true);
}

String LocalAIEngine::composeSettings() noexcept {
    if (!settingsManager.isInitialized()) return "Settings aren't available right now.";

    String body = "Here's a snapshot: personality \"" +
                  personalityManager.getActiveProfile().name +
                  "\", volume " + String(static_cast<unsigned int>(settingsManager.getVolume())) +
                  ", brightness " + String(static_cast<unsigned int>(settingsManager.getScreenBrightness())) +
                  ", language \"" + String(settingsManager.getLanguage()) +
                  "\", 24h clock " + String(settingsManager.getUse24HourClock() ? "on" : "off") + ".";
    return finalize(IntentType::SETTINGS_QUERY, body, false, true);
}

String LocalAIEngine::composeWifi() noexcept {
    if (WiFi.status() == WL_CONNECTED) {
        String body = "WiFi is connected to \"" + WiFi.SSID() + "\" at " +
                      WiFi.localIP().toString() + " with " +
                      String(WiFi.RSSI()) + " dBm signal.";
        return finalize(IntentType::WIFI_STATUS, body, false, true);
    }
    String body = "WiFi isn't connected right now. You can configure it from the web portal.";
    return finalize(IntentType::WIFI_STATUS, body, false, true);
}

String LocalAIEngine::composeStorage() noexcept {
    if (!storageManager.isHealthy()) return "Storage system isn't available right now.";

    const float freeMB = storageManager.getFreeSpace() / (1024.0f * 1024.0f);
    const float totalMB = storageManager.getTotalSpace() / (1024.0f * 1024.0f);
    String body = "Storage has " + String(freeMB, 1) + " MB free of " + String(totalMB, 1) +
                  " MB total. Free heap is " + String(ESP.getFreeHeap() / 1024) + " KB.";

    if (freeMB < 5.0f) {
        body += " " + sentenceEngine.capitalise(
                            sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
                ", space is getting tight - consider clearing old logs.";
    }
    return finalize(IntentType::STORAGE_STATUS, body, false, true);
}

String LocalAIEngine::composePersonality() noexcept {
    if (!personalityManager.isInitialized()) return "Personality system isn't available right now.";

    const PersonalityProfile& profile = personalityManager.getActiveProfile();
    String body = "I'm currently running the \"" + profile.name +
                  "\" personality (" + personalityEngine.styleName() + "). ";
    body += "You can switch profiles from the web portal or ask me to change.";
    return finalize(IntentType::PERSONALITY_QUERY, body, false, true);
}

String LocalAIEngine::composeDecisions() noexcept {
    if (!decisionManager.isInitialized()) return "Decision system isn't available.";

    auto recent = decisionManager.getRecentDecisions(3);
    if (recent.empty()) {
        String body = "No decisions recorded yet. Give me some options and I'll rank them for you.";
        return finalize(IntentType::DECISION_QUERY, body, false, true);
    }

    String body = "Recent decisions: ";
    std::vector<String> lines;
    for (const auto& d : recent) {
        lines.push_back("'" + d.question + "' (" + String(static_cast<unsigned int>(d.overallConfidence * 100)) + "% confidence)");
    }
    body += sentenceEngine.listItems(lines, 3) + ".";
    return finalize(IntentType::DECISION_QUERY, body, false, true);
}

String LocalAIEngine::composeLearning() noexcept {
    if (!learningManager.isInitialized()) return "Learning system isn't available.";

    auto patterns = learningManager.getActivePatterns();
    if (patterns.empty()) {
        String body = "I haven't spotted strong patterns yet. I'll learn from your activity over time.";
        return finalize(IntentType::LEARNING_QUERY, body, false, true);
    }

    String body = "I've noticed " + sentenceEngine.countPhrase(patterns.size(), "pattern") + ": ";
    std::vector<String> lines;
    for (size_t i = 0; i < patterns.size() && i < LOCAL_AI_MAX_DATA_ITEMS; ++i) {
        lines.push_back(patterns[i].name + " (" + String(static_cast<unsigned int>(patterns[i].confidence * 100)) + "% confidence)");
    }
    body += sentenceEngine.listItems(lines, 3) + ".";
    return finalize(IntentType::LEARNING_QUERY, body, false, true);
}

String LocalAIEngine::composeRecommendations() noexcept {
    if (!executiveAssistant.isInitialized()) return "Recommendation system isn't available.";

    auto active = executiveAssistant.getActiveRecommendations();
    if (active.empty()) {
        String body = "I have no active recommendations right now. Ask me about your goals or schedule and I'll suggest next steps.";
        return finalize(IntentType::RECOMMENDATION_QUERY, body, false, true);
    }

    String body = "I have " + sentenceEngine.countPhrase(active.size(), "recommendation") + " for you: ";
    std::vector<String> lines;
    for (size_t i = 0; i < active.size() && i < LOCAL_AI_MAX_DATA_ITEMS; ++i) {
        lines.push_back(active[i].title + " - " + active[i].description);
    }
    body += sentenceEngine.listItems(lines, 3) + ".";

    String advice = recommendationClause();
    if (!advice.isEmpty()) body += " " + advice;
    return finalize(IntentType::RECOMMENDATION_QUERY, body, false, true);
}

String LocalAIEngine::composePredictions() noexcept {
    if (!predictionManager.isInitialized()) return "Prediction system isn't available.";

    auto active = predictionManager.getActivePredictions(0.3f);
    if (active.empty()) {
        String body = String("No significant predictions at the moment. My confidence is ") +
                      sentenceEngine.pickConfidence(0.4f) + ".";
        return finalize(IntentType::PREDICTION_QUERY, body, false, true);
    }

    String body = "My predictions: ";
    std::vector<String> lines;
    for (size_t i = 0; i < active.size() && i < LOCAL_AI_MAX_DATA_ITEMS; ++i) {
        lines.push_back(active[i].targetName + " (" + String(static_cast<unsigned int>(active[i].probability * 100)) + "% probability)");
    }
    body += sentenceEngine.listItems(lines, 3) + ".";
    return finalize(IntentType::PREDICTION_QUERY, body, false, true);
}

String LocalAIEngine::composeDocuments() noexcept {
    if (!documentManager.isInitialized()) return "Document system isn't available.";

    const size_t count = documentManager.documentCount();
    if (count == 0) {
        String body = "You have no documents stored yet. Upload text, markdown, or code from the web portal and I'll index it.";
        return finalize(IntentType::DOCUMENT_QUERY, body, false, true);
    }
    String body = "You have " + sentenceEngine.countPhrase(count, "document") + " stored. ";
    body += "Use the web portal to browse and search them, or ask me a question about their content.";
    return finalize(IntentType::DOCUMENT_QUERY, body, false, true);
}

String LocalAIEngine::composeWorkspaces() noexcept {
    if (!workspaceManager.isInitialized()) return "Workspace system isn't available.";

    auto all = workspaceManager.getAllWorkspaces();
    if (all.empty()) {
        String body = "No workspaces yet. Workspaces group related projects and goals - I can create one for you.";
        return finalize(IntentType::WORKSPACE_QUERY, body, false, true);
    }

    const Workspace* active = workspaceManager.getActiveWorkspace();
    String body = "You have " + sentenceEngine.countPhrase(all.size(), "workspace") + ". ";
    std::vector<String> names;
    for (const auto& w : all) names.push_back(w.name);
    body += sentenceEngine.listItems(names, LOCAL_AI_MAX_DATA_ITEMS) + ".";
    if (active != nullptr) {
        body += " Active: \"" + active->name + "\" with " +
                sentenceEngine.countPhrase(active->members.size(), "member") + ".";
    }
    return finalize(IntentType::WORKSPACE_QUERY, body, false, true);
}

String LocalAIEngine::composeDeveloper() noexcept {
    if (!performanceManager.isInitialized()) return "System monitoring isn't available.";
    return performanceManager.exportDiagnostics();
}

String LocalAIEngine::composeStudy() noexcept {
    if (!studyManager.isInitialized()) return "Study system isn't available right now.";

    auto subjects = studyManager.getAllSubjects();
    if (subjects.empty()) {
        String body = "You haven't created any study subjects yet. Want to create one?";
        return finalize(IntentType::INTENT_STUDY, body, false, true);
    }

    auto due = studyManager.getDueSubjects();
    String body = "You have " + sentenceEngine.countPhrase(subjects.size(), "study subject") +
                  " and " + String(static_cast<unsigned long>(studyManager.totalStudyMinutes())) +
                  " minutes studied. ";
    if (!due.empty()) {
        std::vector<String> dueNames;
        for (const auto& s : due) dueNames.push_back(s.name);
        body += sentenceEngine.capitalise(sentenceEngine.countPhrase(due.size(), "of them")) +
                " are due for review: " + sentenceEngine.listItems(dueNames, 3) + ".";
    } else {
        body += String("Nothing is due for review ") + sentenceEngine.pickEnding(personalityEngine.knobs().style) + ".";
    }

    // Time awareness: almost complete daily target example.
    if (conversationContext.isTimeKnown() && !due.empty()) {
        body += " " + sentenceEngine.capitalise(conversationContext.timeClause()) +
                " - a short session would keep the streak alive.";
    }
    return finalize(IntentType::INTENT_STUDY, body, false, true);
}

String LocalAIEngine::composeFlashcards() noexcept {
    if (!studyManager.isInitialized()) return "Study system isn't available right now.";
    auto subjects = studyManager.getAllSubjects();
    if (subjects.empty()) return "No subjects to draw flashcards from yet.";

    String body = "I can quiz you from " + sentenceEngine.countPhrase(subjects.size(), "subject") + ". ";
    body += "Which subject's flashcards would you like to review?";
    return finalize(IntentType::INTENT_FLASHCARD, body, false, true);
}

String LocalAIEngine::composeQuiz() noexcept {
    if (!studyManager.isInitialized()) return "Study system isn't available right now.";
    String body = "I'm ready to quiz you. Pick a subject and I'll pull due flashcards for a quick session.";
    return finalize(IntentType::INTENT_QUIZ, body, false, true);
}

String LocalAIEngine::composePair() noexcept {
    if (!companionManager.isInitialized()) return "Companion system isn't available right now.";
    String body = "To pair a device, make sure it's on the same network and give me the device name and type - or use the web portal under Companion.";
    return finalize(IntentType::INTENT_PAIR, body, false, true);
}

String LocalAIEngine::composeSync() noexcept {
    if (!companionManager.isInitialized()) return "Companion system isn't available right now.";
    String body = "Sync keeps data in step across paired devices. Manage it from the web portal under Companion.";
    return finalize(IntentType::INTENT_SYNC, body, false, true);
}

String LocalAIEngine::composeDashboard() noexcept {
    String body = "Opening your personal dashboard - an overview of memories, workspaces, recommendations, and today's plan.";
    return finalize(IntentType::INTENT_DASHBOARD, body, false, true);
}

String LocalAIEngine::composeCreateSkill() noexcept {
    if (!skillManager.isInitialized()) return "Skill system isn't available right now.";
    String body = "You can create a skill in the Skill Studio from the web portal - give it a name, a trigger phrase, and define its actions.";
    return finalize(IntentType::INTENT_CREATE_SKILL, body, false, true);
}

// ============================================================================
// Enrichment: memory, knowledge, recommendations, follow-ups, finalize
// ============================================================================

String LocalAIEngine::retrieveMemoryClause(const String& topic) noexcept {
    if (topic.isEmpty() || !memoryManager.isInitialized()) return "";
    auto hits = memoryManager.semanticSearch(topic, 1);
    if (hits.empty()) return "";
    return "I recall: " + hits[0].key + " - " + hits[0].value + ".";
}

String LocalAIEngine::retrieveKnowledgeClause(const String& topic) noexcept {
    if (topic.isEmpty() || !knowledgeGraphManager.isInitialized()) return "";
    auto nodes = knowledgeGraphManager.searchNodes(topic);
    if (nodes.empty()) return "";
    String clause = "Related in my knowledge graph: ";
    clause += nodes[0].name;
    if (!nodes[0].value.isEmpty()) clause += " (" + nodes[0].value + ")";
    clause += ".";
    return clause;
}

String LocalAIEngine::recommendationClause() noexcept {
    RecommendationHint hint = recommendationEngine.build();
    if (!hint.available) return "";
    return "One suggestion: " + hint.advice;
}

String LocalAIEngine::followUpClause(IntentType type) noexcept {
    if (!personalityEngine.knobs().offersFollowUp) return "";
    const char* const* pool = nullptr;
    size_t count = 0;
    switch (type) {
        case IntentType::GREETING:
            pool = kFollowUpGreeting; count = sizeof(kFollowUpGreeting) / sizeof(kFollowUpGreeting[0]);
            break;
        case IntentType::REMINDER_QUERY:
            pool = kFollowUpReminder; count = sizeof(kFollowUpReminder) / sizeof(kFollowUpReminder[0]);
            break;
        case IntentType::PLANNER_QUERY:
            pool = kFollowUpPlanner; count = sizeof(kFollowUpPlanner) / sizeof(kFollowUpPlanner[0]);
            break;
        case IntentType::GOAL_QUERY:
            pool = kFollowUpGoal; count = sizeof(kFollowUpGoal) / sizeof(kFollowUpGoal[0]);
            break;
        case IntentType::HABIT_QUERY:
            pool = kFollowUpHabit; count = sizeof(kFollowUpHabit) / sizeof(kFollowUpHabit[0]);
            break;
        case IntentType::MEMORY_QUERY:
            pool = kFollowUpMemory; count = sizeof(kFollowUpMemory) / sizeof(kFollowUpMemory[0]);
            break;
        case IntentType::KNOWLEDGE_QUERY:
            pool = kFollowUpKnowledge; count = sizeof(kFollowUpKnowledge) / sizeof(kFollowUpKnowledge[0]);
            break;
        case IntentType::INTENT_STUDY:
        case IntentType::INTENT_FLASHCARD:
        case IntentType::INTENT_QUIZ:
            pool = kFollowUpStudy; count = sizeof(kFollowUpStudy) / sizeof(kFollowUpStudy[0]);
            break;
        case IntentType::WORKSPACE_QUERY:
            pool = kFollowUpWorkspace; count = sizeof(kFollowUpWorkspace) / sizeof(kFollowUpWorkspace[0]);
            break;
        default:
            pool = kFollowUpGeneric; count = sizeof(kFollowUpGeneric) / sizeof(kFollowUpGeneric[0]);
            break;
    }
    return pickFollowUp(pool, count);
}

String LocalAIEngine::finalize(IntentType type, String body, bool addClosing, bool addFollowUp) noexcept {
    String response = body;
    if (response.isEmpty()) return response;

    // Response variation: never repeat the last emitted wording.
    if (localAICache.isRecentResponse(response)) {
        response = sentenceEngine.capitalise(sentenceEngine.pickTransition(personalityEngine.knobs().style)) +
                   ", " + sentenceEngine.capitalise(response);
    }

    if (addClosing && personalityEngine.knobs().verbosity > 0) {
        response += String(" ") + sentenceEngine.pickClosing(personalityEngine.knobs().style) + ".";
    }
    if (addFollowUp) {
        const String followUp = followUpClause(type);
        if (!followUp.isEmpty() && response.length() < static_cast<int>(TINYAI_MAX_RESPONSE_LEN) - 80) {
            response += " " + followUp;
        }
    }
    return response;
}

// ============================================================================
// Self-test
// ============================================================================

String LocalAIEngine::runSelfTest() noexcept {
    unsigned long today = static_cast<unsigned long>(time(nullptr) / 86400);
    if (m_lastSelfTestDate == today) return "Self-test already ran today.";
    m_lastSelfTestDate = today;

    IntentResult test;
    struct TestCase {
        IntentType type;
        const char* text;
    };
    const TestCase cases[] = {
        { IntentType::GREETING, "hello" },
        { IntentType::SMALL_TALK, "how are you" },
        { IntentType::CAPABILITIES, "what can you do" },
        { IntentType::REMINDER_QUERY, "my reminders" },
        { IntentType::GOAL_QUERY, "my goals" },
        { IntentType::HABIT_QUERY, "my habits" },
        { IntentType::PLANNER_QUERY, "today's schedule" },
        { IntentType::MEMORY_QUERY, "remember" },
        { IntentType::KNOWLEDGE_QUERY, "knowledge" },
        { IntentType::INTENT_STUDY, "study" },
        { IntentType::WORKSPACE_QUERY, "workspace" },
        { IntentType::UNKNOWN, "zzz nonsense phrase" }
    };

    size_t passed = 0;
    const size_t total = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < total; ++i) {
        test.type = cases[i].type;
        test.confidence = 0.8f;
        String response = generate(test, cases[i].text);
        if (!response.isEmpty() && response.length() <= TINYAI_MAX_RESPONSE_LEN) passed++;
        else LOG_WARNING(kLogCategory, "Self-test case %d failed", static_cast<int>(cases[i].type));
    }

    String report = "Local AI self-test: ";
    report += String(static_cast<unsigned long>(passed));
    report += "/";
    report += String(static_cast<unsigned long>(total));
    report += " cases passed";
    if (passed < total) report += " (some managers not initialized yet)";
    report += ".";
    LOG_INFO(kLogCategory, "%s", report.c_str());
    return report;
}

// ============================================================================
// Status
// ============================================================================

size_t LocalAIEngine::getGeneratedCount() const noexcept {
    return m_generatedCount;
}

unsigned long LocalAIEngine::getLastLatencyMs() const noexcept {
    return m_lastLatencyMs;
}

size_t LocalAIEngine::getCacheSize() const noexcept {
    return localAICache.size();
}

size_t LocalAIEngine::getCacheHits() const noexcept {
    return localAICache.hitCount();
}

String LocalAIEngine::getStatusJSON() const noexcept {
    String json;
    json.reserve(256);
    json += "{\"personality\":\"";
    json += personalityEngine.profileId();
    json += "\",\"style\":\"";
    json += personalityEngine.styleName();
    json += "\",\"generatedCount\":";
    json += String(static_cast<unsigned long>(m_generatedCount));
    json += ",\"lastLatencyMs\":";
    json += String(static_cast<unsigned long>(m_lastLatencyMs));
    json += ",\"cacheSize\":";
    json += String(static_cast<unsigned long>(localAICache.size()));
    json += ",\"topic\":\"";
    json += conversationContext.currentTopic();
    json += "\"}";
    return json;
}
