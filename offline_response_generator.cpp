#include "offline_response_generator.h"
#include <WiFi.h>
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
#include "vault_manager.h"
#include "performance_manager.h"
#include "study_manager.h"
#include "skill_manager.h"
#include "companion_manager.h"

OfflineResponseGenerator::OfflineResponseGenerator() noexcept {}
OfflineResponseGenerator::~OfflineResponseGenerator() noexcept {}

String OfflineResponseGenerator::generate(const IntentResult& intent) noexcept {
    switch (intent.type) {
        case IntentType::GREETING:       return handleGreeting();
        case IntentType::SMALL_TALK:     return handleSmallTalk();
        case IntentType::CAPABILITIES:   return handleCapabilities();
        case IntentType::REMINDER_QUERY: return handleReminderQuery();
        case IntentType::GOAL_QUERY:     return handleGoalQuery();
        case IntentType::HABIT_QUERY:    return handleHabitQuery();
        case IntentType::PLANNER_QUERY:  return handlePlannerQuery();
        case IntentType::MEMORY_QUERY:   return handleMemoryQuery(intent);
        case IntentType::KNOWLEDGE_QUERY:return handleKnowledgeQuery(intent);
        case IntentType::SETTINGS_QUERY: return handleSettingsQuery();
        case IntentType::WIFI_STATUS:    return handleWifiStatus();
        case IntentType::STORAGE_STATUS: return handleStorageStatus();
        case IntentType::PERSONALITY_QUERY: return handlePersonalityQuery();
        case IntentType::DECISION_QUERY: return handleDecisionQuery();
        case IntentType::LEARNING_QUERY: return handleLearningQuery();
        case IntentType::RECOMMENDATION_QUERY: return handleRecommendationQuery();
        case IntentType::PREDICTION_QUERY: return handlePredictionQuery();
        case IntentType::DOCUMENT_QUERY: return handleDocumentQuery();
        case IntentType::WORKSPACE_QUERY: return handleWorkspaceQuery();
        case IntentType::DEVELOPER_QUERY: return handleDeveloperQuery();
        case IntentType::INTENT_STUDY:    return handleStudyQuery();
        case IntentType::INTENT_FLASHCARD: return handleFlashcardQuery();
        case IntentType::INTENT_QUIZ:     return handleQuizQuery();
        case IntentType::INTENT_PAIR:     return handlePairQuery();
        case IntentType::INTENT_SYNC:     return handleSyncQuery();
        case IntentType::INTENT_DASHBOARD: return handleDashboardQuery();
        case IntentType::INTENT_CREATE_SKILL: return handleCreateSkillQuery();
        default:                          return handleUnknown();
    }
}

String OfflineResponseGenerator::handleGreeting() const noexcept {
    String contextSuffix;
    if (contextManager.isInitialized()) {
        auto& ctx = contextManager.getContext();
        if (!ctx.currentProject.isEmpty()) contextSuffix = " I see you're working on " + ctx.currentProject + ".";
        else if (!ctx.currentTask.isEmpty()) contextSuffix = " I see you're working on " + ctx.currentTask + ".";
    }
    const char* greetings[] = {
        "Hello! I'm AURA. How can I help you today?",
        "Hi there! Ready to assist you.",
        "Hey! What can I do for you?",
        "Hello! I'm running in offline mode but I can still help with basic questions."
    };
    return greetings[random(4)] + contextSuffix;
}

String OfflineResponseGenerator::handleSmallTalk() const noexcept {
    String contextInfo;
    if (contextManager.isInitialized()) {
        auto& ctx = contextManager.getContext();
        if (!ctx.currentActivity.isEmpty()) contextInfo = " You've been " + ctx.currentActivity + ".";
        if (!ctx.currentMood.isEmpty()) contextInfo += " You seem " + ctx.currentMood + ".";
    }
    const char* responses[] = {
        "I'm doing well, thank you for asking! I'm running on ESP32 with my offline AI engine.",
        "I'm AURA, your AI desktop assistant. I can help with reminders, goals, habits, and more.",
        "I'm functioning normally. All systems are operational in offline mode.",
        "I'm here and ready to help! What would you like to know?"
    };
    return responses[random(4)] + contextInfo;
}

String OfflineResponseGenerator::handleCapabilities() const noexcept {
    return "I can help with these tasks offline: check your reminders, goals, habits, and schedule. "
           "I can recall past memories, answer questions from my knowledge graph, check WiFi and storage status, "
           "tell you which personality profile is active, and adjust settings. "
           "When connected to the internet, I can answer complex questions using Gemini AI.";
}

String OfflineResponseGenerator::handleReminderQuery() const noexcept {
    if (!reminderManager.isInitialized()) return "Reminder system is not available right now.";

    std::vector<Reminder> reminders;
    size_t count = reminderManager.getReminders(reminders);

    if (count == 0) return "You have no reminders set.";

    std::vector<String> lines;
    for (size_t i = 0; i < count && i < 5; ++i) {
        String line = reminders[i].title;
        if (!reminders[i].message.isEmpty()) {
            line += ": " + reminders[i].message;
        }
        lines.push_back(line);
    }

    String result = "You have " + String(count) + " reminder(s): ";
    result += joinStrings(lines, "; ");
    if (count > 5) {
        result += " (and " + String(count - 5) + " more)";
    }
    return result;
}

String OfflineResponseGenerator::handleGoalQuery() const noexcept {
    if (!goalManager.isInitialized()) return "Goal system is not available right now.";

    auto goals = goalManager.getActiveGoals();
    if (goals.empty()) return "You have no active goals.";

    std::vector<String> lines;
    for (size_t i = 0; i < goals.size() && i < 5; ++i) {
        String line = goals[i].title;
        if (!goals[i].description.isEmpty()) {
            line += " - " + goals[i].description;
        }
        lines.push_back(line);
    }

    String result = "Your active goals: ";
    result += joinStrings(lines, "; ");
    size_t total = goalManager.getAllGoals().size();
    if (total > goals.size()) {
        result += ". You also have " + String(total - goals.size()) + " completed or inactive goals.";
    }
    return result;
}

String OfflineResponseGenerator::handleHabitQuery() const noexcept {
    if (!habitManager.isInitialized()) return "Habit system is not available right now.";

    auto habits = habitManager.getAllHabits();
    if (habits.empty()) return "You have no habits tracked.";

    std::vector<String> lines;
    for (size_t i = 0; i < habits.size() && i < 5; ++i) {
        lines.push_back(habits[i].name);
    }

    String result = "You have " + String(habits.size()) + " habit(s): ";
    result += joinStrings(lines, "; ");

    auto due = habitManager.getDueHabits();
    if (!due.empty()) {
        result += ". " + String(due.size()) + " habit(s) are due today.";
    }
    return result;
}

String OfflineResponseGenerator::handlePlannerQuery() const noexcept {
    if (!plannerManager.isInitialized()) return "Planner is not available right now.";

    auto tasks = plannerManager.getTodaysTasks();
    if (tasks.empty()) {
        tasks = plannerManager.getUpcomingTasks();
    }

    if (tasks.empty()) return "You have no tasks scheduled.";

    std::vector<String> lines;
    for (size_t i = 0; i < tasks.size() && i < 5; ++i) {
        String line = tasks[i].title;
        if (tasks[i].deadline > 0) {
            line += " (due: " + String(tasks[i].deadline) + ")";
        }
        lines.push_back(line);
    }

    String result = "Your tasks: ";
    result += joinStrings(lines, "; ");
    if (tasks.size() > 5) {
        result += " (and " + String(tasks.size() - 5) + " more)";
    }
    return result;
}

String OfflineResponseGenerator::handleMemoryQuery(const IntentResult& intent) const noexcept {
    if (!memoryManager.isInitialized()) return "Memory system is not available right now.";

    String response;
    String query;

    if (intent.entityCount > 0) {
        query = intent.entities[0];
    }

    if (query.isEmpty() || query == "memory" || query == "remember") {
        auto ranked = memoryManager.getRankedMemories(3);
        if (ranked.empty()) return "I don't have any stored memories yet.";

        std::vector<String> lines;
        for (size_t i = 0; i < ranked.size(); ++i) {
            lines.push_back(ranked[i].key + ": " + ranked[i].value);
        }
        response = "My top memories: " + joinStrings(lines, "; ");
    } else {
        auto results = memoryManager.search(query, false);
        if (results.empty()) {
            auto convos = memoryManager.searchConversations(query);
            if (convos.empty()) {
                response = "I couldn't find anything about \"" + query + "\" in my memory.";
            } else {
                response = "From past conversations about \"" + query + "\": " + convos[0].summary;
            }
        } else {
            std::vector<String> lines;
            for (size_t i = 0; i < results.size() && i < 3; ++i) {
                lines.push_back(results[i].key + ": " + results[i].value);
            }
            response = "I found " + String(results.size()) + " memory/memories about \"" + query + "\": " + joinStrings(lines, "; ");
        }
    }

    if (response.isEmpty()) {
        response = "I couldn't find anything matching that in my memory.";
    }
    return response;
}

String OfflineResponseGenerator::handleKnowledgeQuery(const IntentResult& intent) const noexcept {
    if (!knowledgeGraphManager.isInitialized()) return "Knowledge graph is not available right now.";

    String query;
    if (intent.entityCount > 0) {
        query = intent.entities[0];
    }

    if (query.isEmpty()) {
        size_t nodeCount = knowledgeGraphManager.nodeCount();
        size_t edgeCount = knowledgeGraphManager.edgeCount();
        return "My knowledge graph contains " + String(nodeCount) + " node(s) and " + String(edgeCount) + " connection(s).";
    }

    auto nodes = knowledgeGraphManager.searchNodes(query);
    if (nodes.empty()) {
        return "I don't have any information about \"" + query + "\" in my knowledge graph.";
    }

    std::vector<String> lines;
    for (size_t i = 0; i < nodes.size() && i < 3; ++i) {
        String info = nodes[i].name;
        if (!nodes[i].value.isEmpty()) {
            info += ": " + nodes[i].value;
        }
        lines.push_back(info);
    }

    String result = "From my knowledge graph about \"" + query + "\": ";
    result += joinStrings(lines, "; ");
    return result;
}

String OfflineResponseGenerator::handleSettingsQuery() const noexcept {
    if (!settingsManager.isInitialized()) return "Settings are not available right now.";

    String result = "Current settings: ";
    result += "Personality: " + personalityManager.getActiveProfile().name + ". ";
    result += "Volume: " + String(settingsManager.getVolume()) + ". ";
    result += "Screen brightness: " + String(settingsManager.getScreenBrightness()) + ". ";
    result += "WiFi auto-connect: " + String(settingsManager.getWifiAutoConnect() ? "enabled" : "disabled") + ". ";
    result += "Language: " + String(settingsManager.getLanguage()) + ". ";
    result += "Time zone: UTC " + String(settingsManager.getTimezone());
    return result;
}

String OfflineResponseGenerator::handleWifiStatus() const noexcept {
    if (WiFi.status() == WL_CONNECTED) {
        return "WiFi is connected. SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString() +
               ", Signal: " + String(WiFi.RSSI()) + " dBm";
    }
    return "WiFi is not connected. You can configure WiFi from the web portal or use offline mode.";
}

String OfflineResponseGenerator::handleStorageStatus() const noexcept {
    if (!storageManager.isHealthy()) return "Storage system is not available right now.";

    size_t totalBytes = storageManager.getTotalSpace();
    size_t freeBytes = storageManager.getFreeSpace();
    size_t usedBytes = totalBytes - freeBytes;

    float freeMB = freeBytes / (1024.0f * 1024.0f);
    float totalMB = totalBytes / (1024.0f * 1024.0f);

    return "Storage: " + String(freeMB, 1) + " MB free of " + String(totalMB, 1) + " MB total. "
           "Free heap: " + String(ESP.getFreeHeap() / 1024) + " KB.";
}

String OfflineResponseGenerator::handlePersonalityQuery() const noexcept {
    if (!personalityManager.isInitialized()) return "Personality system is not available right now.";

    return "The active personality is \"" + personalityManager.getActiveProfile().name + "\". "
           "You can change it from the web portal or ask me to switch.";
}

String OfflineResponseGenerator::handleDecisionQuery() const noexcept {
    if (!decisionManager.isInitialized()) return "Decision system is not available.";
    auto recent = decisionManager.getRecentDecisions(3);
    if (recent.empty()) return "No recent decisions recorded. I can help you compare options if you provide them.";
    String result = "Recent decisions: ";
    for (size_t i = 0; i < recent.size(); ++i) {
        if (i > 0) result += "; ";
        result += "'" + recent[i].question + "' -> confidence: " + String(recent[i].overallConfidence * 100, 0) + "%";
    }
    return result;
}

String OfflineResponseGenerator::handleLearningQuery() const noexcept {
    if (!learningManager.isInitialized()) return "Learning system is not available.";
    auto patterns = learningManager.getActivePatterns();
    if (patterns.empty()) return "I haven't detected any patterns yet. I'll learn from your activities over time.";
    String result = "I've noticed " + String(patterns.size()) + " pattern(s): ";
    for (size_t i = 0; i < patterns.size() && i < 5; ++i) {
        if (i > 0) result += "; ";
        result += patterns[i].name + " (" + String(patterns[i].confidence * 100, 0) + "% confidence)";
    }
    return result;
}

String OfflineResponseGenerator::handleRecommendationQuery() const noexcept {
    if (!executiveAssistant.isInitialized()) return "Recommendation system is not available.";
    auto active = executiveAssistant.getActiveRecommendations();
    if (active.empty()) return "No active recommendations right now.";
    String result = "I have " + String(active.size()) + " recommendation(s) for you: ";
    for (size_t i = 0; i < active.size() && i < 5; ++i) {
        if (i > 0) result += "; ";
        result += active[i].title + ": " + active[i].description;
    }
    return result;
}

String OfflineResponseGenerator::handlePredictionQuery() const noexcept {
    if (!predictionManager.isInitialized()) return "Prediction system is not available.";
    auto active = predictionManager.getActivePredictions(0.3f);
    if (active.empty()) return "No significant predictions at this time.";
    String result = "My predictions: ";
    for (size_t i = 0; i < active.size() && i < 5; ++i) {
        if (i > 0) result += "; ";
        result += active[i].targetName + ": " + String(active[i].probability * 100, 0) + "% probability";
    }
    return result;
}

String OfflineResponseGenerator::handleDocumentQuery() const noexcept {
    if (!documentManager.isInitialized()) return "Document system is not available.";
    size_t count = documentManager.documentCount();
    if (count == 0) return "You have no stored documents yet. You can upload text, markdown, or code files.";
    return "You have " + String(count) + " document(s) stored. Use the web portal to browse and search them.";
}

String OfflineResponseGenerator::handleWorkspaceQuery() const noexcept {
    if (!workspaceManager.isInitialized()) return "Workspace system is not available.";
    auto all = workspaceManager.getAllWorkspaces();
    if (all.empty()) return "No workspaces created yet. Workspaces help you organize related projects and goals.";
    auto active = workspaceManager.getActiveWorkspace();
    String result = "You have " + String(all.size()) + " workspace(s).";
    if (active) result += " Active: '" + active->name + "' with " + String(active->members.size()) + " member(s).";
    return result;
}

String OfflineResponseGenerator::handleDeveloperQuery() const noexcept {
    if (!performanceManager.isInitialized()) return "System monitoring is not available.";
    return performanceManager.exportDiagnostics();
}

String OfflineResponseGenerator::handleStudyQuery() const noexcept {
    if (!studyManager.isInitialized()) return "Study system is not available right now.";
    auto subjects = studyManager.getAllSubjects();
    if (subjects.empty()) {
        return "You haven't created any study subjects yet. Would you like to create one?";
    }
    String response = "You have " + String(subjects.size()) + " study subjects. ";
    response += "Your total study time is " + String(studyManager.totalStudyMinutes()) + " minutes.";
    return response;
}

String OfflineResponseGenerator::handleFlashcardQuery() const noexcept {
    if (!studyManager.isInitialized()) return "Study system is not available right now.";
    auto subjects = studyManager.getAllSubjects();
    if (subjects.empty()) return "No study subjects available to show flashcards for.";
    return "You have " + String(subjects.size()) + " subject(s). Which subject's flashcards would you like to see?";
}

String OfflineResponseGenerator::handleQuizQuery() const noexcept {
    if (!studyManager.isInitialized()) return "Study system is not available right now.";
    return "I can quiz you on your study subjects. Please specify which subject you'd like to be quizzed on.";
}

String OfflineResponseGenerator::handlePairQuery() const noexcept {
    if (!companionManager.isInitialized()) return "Companion system is not available right now.";
    return "To pair a device, make sure it's on the same network and provide the device name and type. You can also use the web portal under Companion.";
}

String OfflineResponseGenerator::handleSyncQuery() const noexcept {
    if (!companionManager.isInitialized()) return "Companion system is not available right now.";
    return "Sync lets you keep your data in sync across paired devices. You can manage sync from the web portal.";
}

String OfflineResponseGenerator::handleDashboardQuery() const noexcept {
    return "Opening your personal dashboard. You'll find an overview of your memories, workspaces, recommendations, and more.";
}

String OfflineResponseGenerator::handleCreateSkillQuery() const noexcept {
    if (!skillManager.isInitialized()) return "Skill system is not available right now.";
    return "You can create a new skill from the Skill Studio in the web portal. Give it a name, trigger phrase, and define its actions.";
}

String OfflineResponseGenerator::handleUnknown() const noexcept {
    const char* responses[] = {
        "I'm not sure I understand. I can help with reminders, goals, habits, your schedule, memories, and settings. Try asking \"What can you do?\"",
        "I didn't quite catch that. I work best with specific questions about your reminders, goals, habits, or schedule.",
        "I'm in offline mode and can handle basic queries. Try asking about your reminders, goals, or what I can do.",
        "Sorry, I didn't understand that. In offline mode I can check your reminders, goals, habits, schedule, memories, and settings."
    };
    return responses[random(4)];
}

String OfflineResponseGenerator::joinStrings(const std::vector<String>& items, const String& separator) const noexcept {
    if (items.empty()) return "";
    String result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) result += separator;
        result += items[i];
    }
    return result;
}
