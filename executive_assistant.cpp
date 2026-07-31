#include "executive_assistant.h"
#include "logger.h"
#include "storage_manager.h"
#include "event_bus.h"
#include "json_helpers.h"
#include "conversation_manager.h"
#include "study_manager.h"
#include "workspace_manager.h"
#include "habit_manager.h"
#include "performance_manager.h"
#include "learning_manager.h"
#include "analytics_manager.h"
#include "context_manager.h"
#include "reminder_manager.h"
#include "memory_manager.h"
#include <WiFi.h>
#include <algorithm>

ExecutiveAssistant executiveAssistant;

ExecutiveAssistant::ExecutiveAssistant() noexcept
    : m_initialized(false)
    , m_lastCheckTime(0)
    , m_dailyBriefShown(false)
    , m_lastBriefDate(0)
    , m_lastPatternObservation(0)
    , m_lastRecIdCounter(0)
    , m_lastRecGenTime(0)
    , m_explanationDetailLevel(1)
    , m_recsDirty(false) {
    for (auto& c : m_categoryCooldowns) c = 0;
    for (auto& a : m_acceptedCount) a = 0;
    for (auto& d : m_dismissedCount) d = 0;
    m_recommendations.reserve(kMaxActiveRecs + kMaxHistoryRecs);
}

ExecutiveAssistant::~ExecutiveAssistant() noexcept = default;

bool ExecutiveAssistant::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    m_lastCheckTime = millis();
    LOG_INFO(kLogCategory, "Initialized");
    loadRecommendations();
    return true;
}

void ExecutiveAssistant::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastCheckTime < kCheckIntervalMs) return;
    m_lastCheckTime = now;

    if (conversationManager.isBusy()) return;

    pruneExpired();
    checkStudyRoutine();
    checkProjectUpdates();
    checkUpcomingReminders();
    checkWiFiHealth();
    checkStorageHealth();
    checkBackupReminder();
    checkHabitReminders();
    checkFocusTime();
    generateDailyBrief();
    generateProductivityInsight();

    // Recommendation generation
    if (now - m_lastRecGenTime >= kRecGenIntervalMs) {
        m_lastRecGenTime = now;
        generateAllRecommendations();
    }
    cleanExpiredRecs();
    if (m_recsDirty) {
        saveRecommendations();
        m_recsDirty = false;
    }
}

std::vector<Suggestion> ExecutiveAssistant::getActiveSuggestions(uint8_t minPriority) const noexcept {
    std::vector<Suggestion> active;
    for (const auto& s : m_suggestions) {
        if (s.priority >= minPriority && !s.shown && millis() >= s.cooldownEnd) {
            active.push_back(s);
        }
    }
    return active;
}

void ExecutiveAssistant::dismissSuggestion(const String& suggestionId) noexcept {
    for (auto it = m_suggestions.begin(); it != m_suggestions.end(); ++it) {
        if (it->id == suggestionId) {
            it->shown = true;
            return;
        }
    }
}

void ExecutiveAssistant::snoozeSuggestion(const String& suggestionId, unsigned long durationMs) noexcept {
    for (auto it = m_suggestions.begin(); it != m_suggestions.end(); ++it) {
        if (it->id == suggestionId) {
            it->cooldownEnd = millis() + durationMs;
            return;
        }
    }
}

void ExecutiveAssistant::suggest(const String& title, const String& description, uint8_t priority, SuggestionCategory category) noexcept {
    if (m_suggestions.size() >= kMaxSuggestions) {
        m_suggestions.erase(m_suggestions.begin());
    }

    for (const auto& s : m_suggestions) {
        if (s.title == title && !s.shown) return;
    }

    Suggestion sug;
    sug.id = "sug_" + String(millis());
    sug.title = title;
    sug.description = description;
    sug.priority = priority;
    sug.timestamp = millis();
    sug.actionable = true;
    sug.shown = false;
    sug.cooldownEnd = 0;
    sug.category = category;

    uint8_t catIdx = static_cast<uint8_t>(category);
    if (catIdx < 14) {
        m_categoryCooldowns[catIdx] = millis() + kSuggestionCooldownMs;
    }

    m_suggestions.push_back(sug);

    if (eventBus.isInitialized()) {
        String data = "{\"title\":\"" + title + "\",\"priority\":" + String(priority) + "}";
        eventBus.publish(EventType::EXECUTIVE_SUGGESTION, "ExecutiveAssistant", data);
    }

    LOG_INFO(kLogCategory, "Suggestion: %s (p%d)", title.c_str(), priority);
}

bool ExecutiveAssistant::isInitialized() const noexcept {
    return m_initialized;
}

void ExecutiveAssistant::checkStudyRoutine() noexcept {
    if (!studyManager.isInitialized()) return;
    auto subjects = studyManager.getDueSubjects();
    if (!subjects.empty()) {
        for (const auto& subj : subjects) {
            String msg = subj.name + " revision is pending";
            suggest("Study Time", msg, 2, SuggestionCategory::STUDY_REMINDER);
        }
    }
    if (learningManager.isInitialized()) {
        auto patterns = learningManager.getActivePatterns(0.7f);
        for (const auto& p : patterns) {
            if (p.type == LearningPatternType::STUDY_HABIT && !p.suggestion.isEmpty()) {
                suggest("Study Routine", p.suggestion, 1, SuggestionCategory::ROUTINE_SUGGESTION);
            }
        }
    }
}

void ExecutiveAssistant::checkProjectUpdates() noexcept {
    if (!workspaceManager.isInitialized()) return;
    auto ws = workspaceManager.getAllWorkspaces();
    unsigned long now = millis();
    for (const auto& w : ws) {
        if (w.active && now - w.updatedAt > 86400000UL) {
            String msg = w.name + " hasn't been updated today";
            suggest("Project Update", msg, 1, SuggestionCategory::PROJECT_UPDATE);
        }
    }
}

void ExecutiveAssistant::checkUpcomingReminders() noexcept {
    if (!reminderManager.isInitialized()) return;
    std::vector<Reminder> reminders;
    reminderManager.getReminders(reminders);
    for (const auto& rem : reminders) {
        if (rem.status == ReminderStatus::PENDING || rem.status == ReminderStatus::ACTIVE) {
            unsigned long timeUntil = rem.triggerTime - (millis() / 1000);
            if (timeUntil > 0 && timeUntil < 900) {
                String msg = rem.title + " due in " + String(timeUntil / 60) + " minutes";
                suggest("Upcoming Reminder", msg, 3, SuggestionCategory::REMINDER_UPCOMING);
            }
        }
    }
}

void ExecutiveAssistant::checkWiFiHealth() noexcept {
    if (WiFi.isConnected()) {
        int32_t rssi = WiFi.RSSI();
        if (rssi < -80) {
            unsigned long now = millis();
            static unsigned long wifiWarnStart = 0;
            if (wifiWarnStart == 0) wifiWarnStart = now;
            else if (now - wifiWarnStart > 1200000) {
                suggest("Weak WiFi", "WiFi signal weak for 20+ minutes", 2, SuggestionCategory::WIFI_ISSUE);
                wifiWarnStart = now;
            }
        }
    }
}

void ExecutiveAssistant::checkStorageHealth() noexcept {
    if (storageManager.isInitialized() && storageManager.isSDMounted()) {
        size_t totalBytes = 0, usedBytes = 0, freeBytes = 0;
        storageManager.getStatistics(StorageType::SD_CARD, totalBytes, usedBytes, freeBytes);
        if (totalBytes > 0 && (float)freeBytes / (float)totalBytes < 0.1f) {
            suggest("Storage Alert", "SD storage nearly full", 3, SuggestionCategory::STORAGE_ISSUE);
        }
    }
}

void ExecutiveAssistant::checkBackupReminder() noexcept {
    if (memoryManager.isInitialized()) {
        static unsigned long lastBackupCheck = 0;
        unsigned long now = millis();
        if (lastBackupCheck == 0) {
            lastBackupCheck = now;
            return;
        }
        if (now - lastBackupCheck > 604800000UL) {
            suggest("Backup Reminder", "No recent backup detected", 1, SuggestionCategory::BACKUP_REMINDER);
            lastBackupCheck = now;
        }
    }
}

void ExecutiveAssistant::checkHabitReminders() noexcept {
    if (!habitManager.isInitialized()) return;
    if (learningManager.isInitialized()) {
        auto patterns = learningManager.getActivePatterns(0.6f);
        for (const auto& p : patterns) {
            if (p.type == LearningPatternType::DAILY_ROUTINE && p.actionable) {
                suggest("Habit", p.suggestion, 1, SuggestionCategory::HABIT_REMINDER);
            }
        }
    }
}

void ExecutiveAssistant::checkFocusTime() noexcept {
    unsigned long now = millis();
    static unsigned long focusCheckStart = 0;
    if (focusCheckStart == 0) {
        focusCheckStart = now;
        return;
    }
    if (now - focusCheckStart > 7200000) {
        suggest("Focus Break", "You have been working for over 2 hours", 1, SuggestionCategory::FOCUS_SUGGESTION);
        focusCheckStart = now;
    }
}

void ExecutiveAssistant::generateDailyBrief() noexcept {
    unsigned long now = millis();
    unsigned long today = now / 86400000UL;
    if (m_dailyBriefShown && m_lastBriefDate == today) return;

    if (contextManager.isInitialized()) {
        const auto& ctx = contextManager.getContext();
        m_lastBriefDate = today;
        m_dailyBriefShown = true;

        String brief = "Daily Brief: " + String(ctx.conversationCount) + " conversations";
        if (analyticsManager.isInitialized()) {
            brief += ", " + String(static_cast<int>(analyticsManager.getTotalStudyMinutes())) + " min study";
        }
        suggest("Daily Brief", brief, 1, SuggestionCategory::DAILY_BRIEF);

        if (eventBus.isInitialized()) {
            eventBus.publish(EventType::EXECUTIVE_DAILY_BRIEF, "ExecutiveAssistant",
                             "{\"brief\":\"" + brief + "\"}");
        }
    }
}

void ExecutiveAssistant::generateProductivityInsight() noexcept {
    if (!analyticsManager.isInitialized()) return;
    unsigned long now = millis();

    auto studyTrend = analyticsManager.getTrend("study", "minutes");
    if (studyTrend.dataPoints > 5 && !studyTrend.increasing) {
        if (!isCooldownActive(SuggestionCategory::PRODUCTIVITY_INSIGHT)) {
            suggest("Productivity Insight", "Study time has been declining", 2, SuggestionCategory::PRODUCTIVITY_INSIGHT);
            if (eventBus.isInitialized()) {
                eventBus.publish(EventType::EXECUTIVE_INSIGHT, "ExecutiveAssistant",
                                 "{\"insight\":\"Study time declining\"}");
            }
        }
    }
}

void ExecutiveAssistant::pruneExpired() noexcept {
    unsigned long now = millis();
    for (auto it = m_suggestions.begin(); it != m_suggestions.end(); ) {
        if (it->shown && now - it->timestamp > 86400000UL) {
            it = m_suggestions.erase(it);
        } else {
            ++it;
        }
    }
}

void ExecutiveAssistant::recordUserResponse(const String& suggestionId, bool accepted) noexcept {
    for (const auto& s : m_suggestions) {
        if (s.id == suggestionId) {
            uint8_t catIdx = static_cast<uint8_t>(s.category);
            if (catIdx < 14) {
                if (accepted) {
                    m_acceptedCount[catIdx]++;
                    // User accepted — shorten future cooldown for this category
                    unsigned long now = millis();
                    unsigned long remaining = (m_categoryCooldowns[catIdx] > now)
                        ? m_categoryCooldowns[catIdx] - now : 0;
                    m_categoryCooldowns[catIdx] = now + (remaining / 2);
                } else {
                    m_dismissedCount[catIdx]++;
                }
            }
            return;
        }
    }
}

unsigned long ExecutiveAssistant::getAdaptiveCooldown(SuggestionCategory category) const noexcept {
    uint8_t idx = static_cast<uint8_t>(category);
    if (idx >= 14) return kSuggestionCooldownMs;

    float acceptRate = 0.5f;
    uint8_t total = m_acceptedCount[idx] + m_dismissedCount[idx];
    if (total > 0) {
        acceptRate = static_cast<float>(m_acceptedCount[idx]) / static_cast<float>(total);
    }

    // If user accepts often, reduce cooldown; if dismisses often, increase it
    if (acceptRate > 0.7f) return kSuggestionCooldownMs / 2;
    if (acceptRate < 0.2f) return kSuggestionCooldownMs * 2;
    return kSuggestionCooldownMs;
}

bool ExecutiveAssistant::isCooldownActive(SuggestionCategory category) const noexcept {
    uint8_t idx = static_cast<uint8_t>(category);
    if (idx >= 14) return false;
    unsigned long effectiveCooldown = getAdaptiveCooldown(category);
    unsigned long cooldownEnd = m_categoryCooldowns[idx];
    if (cooldownEnd > 0 && (cooldownEnd - millis()) < effectiveCooldown) {
        cooldownEnd = millis() + effectiveCooldown;
    }
    return millis() < cooldownEnd;
}

// ============================================================================
// Recommendation API (merged from RecommendationManager)
// ============================================================================

void ExecutiveAssistant::generateAllRecommendations() noexcept {
    generateStudySuggestion();
    generateBreakReminder();
    generateProjectContinuation();
    generateMemoryReview();
    generateDeadlineReminder();
    generateHealthReminder();
}

String ExecutiveAssistant::generateStudySuggestion() noexcept {
    return addRecommendation(RecommendationCategory::STUDY_SUGGESTION,
        "Study Session", "Consider starting a focused study session to maintain your learning momentum.",
        0.4f, millis() + 86400000UL);
}

String ExecutiveAssistant::generateBreakReminder() noexcept {
    return addRecommendation(RecommendationCategory::BREAK_REMINDER,
        "Take a Break", "You've been active for a while. A short break can improve focus and productivity.",
        0.3f, millis() + 3600000UL);
}

String ExecutiveAssistant::generateProjectContinuation() noexcept {
    return addRecommendation(RecommendationCategory::PROJECT_CONTINUATION,
        "Continue Project", "Pick up where you left off on your current project to maintain progress.",
        0.5f, millis() + 86400000UL);
}

String ExecutiveAssistant::generateMemoryReview() noexcept {
    return addRecommendation(RecommendationCategory::MEMORY_REVIEW,
        "Review Memories", "Review recent memories to reinforce important information.",
        0.2f, millis() + 604800000UL);
}

String ExecutiveAssistant::generateDeadlineReminder() noexcept {
    return addRecommendation(RecommendationCategory::UPCOMING_DEADLINE,
        "Upcoming Deadline", "Check your planner for tasks with approaching deadlines.",
        0.6f, millis() + 86400000UL);
}

String ExecutiveAssistant::generateHealthReminder() noexcept {
    return addRecommendation(RecommendationCategory::HEALTH_REMINDER,
        "Health Check", "Remember to stay hydrated and maintain good posture while working.",
        0.3f, millis() + 7200000UL);
}

String ExecutiveAssistant::addRecommendation(RecommendationCategory cat, const String& title,
                                              const String& description, float priority,
                                              unsigned long expiry) noexcept {
    if (!m_initialized) return "";

    for (auto& r : m_recommendations) {
        if (!r.dismissed && !r.acted && r.title == title) {
            r.timestamp = millis();
            return r.id;
        }
    }

    size_t activeCount = 0;
    float minPrio = 1.0f;
    size_t minIdx = 0;
    for (size_t i = 0; i < m_recommendations.size(); ++i) {
        if (!m_recommendations[i].dismissed && !m_recommendations[i].acted) {
            activeCount++;
            if (m_recommendations[i].priority < minPrio) {
                minPrio = m_recommendations[i].priority;
                minIdx = i;
            }
        }
    }
    if (activeCount >= kMaxActiveRecs) {
        m_recommendations[minIdx].dismissed = true;
    }

    Recommendation r;
    r.id = generateRecId();
    r.timestamp = millis();
    r.category = cat;
    r.title = title;
    r.description = description;
    r.priority = constrain(priority, 0.0f, 1.0f);
    r.expiry = expiry;
    m_recommendations.push_back(r);
    trimRecHistory();
    m_recsDirty = true;
    return r.id;
}

void ExecutiveAssistant::dismissRecommendation(const String& id) noexcept {
    for (auto& r : m_recommendations) {
        if (r.id == id) { r.dismissed = true; m_recsDirty = true; break; }
    }
}

void ExecutiveAssistant::markRecommendationActed(const String& id) noexcept {
    for (auto& r : m_recommendations) {
        if (r.id == id) { r.acted = true; m_recsDirty = true; break; }
    }
}

std::vector<Recommendation> ExecutiveAssistant::getActiveRecommendations() const noexcept {
    std::vector<Recommendation> active;
    for (const auto& r : m_recommendations) {
        if (!r.dismissed && !r.acted) active.push_back(r);
    }
    return active;
}

std::vector<Recommendation> ExecutiveAssistant::getRecommendationHistory(size_t count) const noexcept {
    std::vector<Recommendation> history;
    size_t start = (m_recommendations.size() > count) ? m_recommendations.size() - count : 0;
    for (size_t i = start; i < m_recommendations.size(); ++i) {
        history.push_back(m_recommendations[i]);
    }
    return history;
}

std::vector<Recommendation> ExecutiveAssistant::getRecommendationsByCategory(RecommendationCategory cat) const noexcept {
    std::vector<Recommendation> results;
    for (const auto& r : m_recommendations) {
        if (r.category == cat) results.push_back(r);
    }
    return results;
}

String ExecutiveAssistant::getRecommendationsJson(bool activeOnly) const noexcept {
    String json; json.reserve(4096);
    json += "{\"recommendations\":[";
    bool first = true;
    for (const auto& r : m_recommendations) {
        if (activeOnly && (r.dismissed || r.acted)) continue;
        if (!first) json += ",";
        first = false;
        json += serializeRec(r);
    }
    json += "]}";
    return json;
}

String ExecutiveAssistant::explainRecommendation(const String& recId) const noexcept {
    for (const auto& r : m_recommendations) {
        if (r.id != recId) continue;
        if (m_explanationDetailLevel == 0) {
            return r.description;
        }
        String detail;
        detail.reserve(512);
        if (m_explanationDetailLevel >= 2) {
            detail += "Recommendation: " + r.title + "\n";
            detail += "Type: " + String(recCategoryToString(r.category)) + "\n";
            detail += "Confidence: " + r.confidence + "\n";
            detail += "Why: " + r.explanation + "\n";
            detail += "Based on: " + r.sourceData + "\n";
            detail += "Relevance: " + String(r.relevanceScore, 2) + "\n";
            detail += "Priority: " + String(r.priority, 2);
        } else {
            detail += "Recommendation: " + r.title + "\n";
            detail += "Type: " + String(recCategoryToString(r.category)) + "\n";
            detail += "Confidence: " + r.confidence + "\n";
            detail += "Why: " + r.explanation + "\n";
            detail += "Based on: " + r.sourceData;
        }
        return detail;
    }
    return "Recommendation not found.";
}

std::vector<Recommendation> ExecutiveAssistant::getRecommendationsByConfidence(const String& confidenceLevel) const noexcept {
    std::vector<Recommendation> results;
    for (const auto& r : m_recommendations) {
        if (r.confidence == confidenceLevel) results.push_back(r);
    }
    return results;
}

void ExecutiveAssistant::setExplanationDetailLevel(uint8_t level) noexcept {
    m_explanationDetailLevel = (level > 2) ? 2 : level;
}

bool ExecutiveAssistant::saveRecommendations() noexcept {
    String path = String(RECOMMENDATIONS_PATH) + "/data.json";
    String json; json.reserve(8192);
    json += "{\"version\":1,\"recommendations\":[";
    for (size_t i = 0; i < m_recommendations.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeRec(m_recommendations[i]);
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    return (st == StorageStatus::SUCCESS);
}

bool ExecutiveAssistant::loadRecommendations() noexcept {
    String path = String(RECOMMENDATIONS_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS ||
        content.isEmpty()) return false;
    m_recommendations.clear();
    int recStart = content.indexOf("\"recommendations\":[");
    if (recStart < 0) return false;
    int pos = content.indexOf('[', recStart) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        Recommendation r = deserializeRec(obj);
        if (!r.id.isEmpty()) m_recommendations.push_back(r);
        pos = braceEnd + 1;
    }
    return true;
}

String ExecutiveAssistant::generateRecId() noexcept {
    unsigned long now = millis(); m_lastRecIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastRecIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) {
        id += hex[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (m_lastRecIdCounter + i);
    }
    return id;
}

void ExecutiveAssistant::trimRecHistory() noexcept {
    while (m_recommendations.size() > kMaxHistoryRecs) {
        m_recommendations.erase(m_recommendations.begin());
    }
}

void ExecutiveAssistant::cleanExpiredRecs() noexcept {
    unsigned long now = millis();
    for (auto& r : m_recommendations) {
        if (!r.dismissed && !r.acted && r.expiry > 0 && now >= r.expiry) {
            r.dismissed = true;
            m_recsDirty = true;
        }
    }
}

const char* ExecutiveAssistant::recCategoryToString(RecommendationCategory c) const noexcept {
    switch (c) {
        case RecommendationCategory::STUDY_SUGGESTION: return "study_suggestion";
        case RecommendationCategory::BREAK_REMINDER: return "break_reminder";
        case RecommendationCategory::PROJECT_CONTINUATION: return "project_continuation";
        case RecommendationCategory::MEMORY_REVIEW: return "memory_review";
        case RecommendationCategory::UPCOMING_DEADLINE: return "upcoming_deadline";
        case RecommendationCategory::HEALTH_REMINDER: return "health_reminder";
        case RecommendationCategory::HABIT_SUGGESTION: return "habit_suggestion";
        case RecommendationCategory::GOAL_SUGGESTION: return "goal_suggestion";
        default: return "custom";
    }
}

RecommendationCategory ExecutiveAssistant::recStringToCategory(const String& s) const noexcept {
    if (s == "study_suggestion") return RecommendationCategory::STUDY_SUGGESTION;
    if (s == "break_reminder") return RecommendationCategory::BREAK_REMINDER;
    if (s == "project_continuation") return RecommendationCategory::PROJECT_CONTINUATION;
    if (s == "memory_review") return RecommendationCategory::MEMORY_REVIEW;
    if (s == "upcoming_deadline") return RecommendationCategory::UPCOMING_DEADLINE;
    if (s == "health_reminder") return RecommendationCategory::HEALTH_REMINDER;
    if (s == "habit_suggestion") return RecommendationCategory::HABIT_SUGGESTION;
    if (s == "goal_suggestion") return RecommendationCategory::GOAL_SUGGESTION;
    return RecommendationCategory::CUSTOM;
}

String ExecutiveAssistant::serializeRec(const Recommendation& r) const noexcept {
    String j; j.reserve(512);
    j += "{";
    j += "\"id\":\"" + escapeJson(r.id) + "\",";
    j += "\"ts\":" + String(r.timestamp) + ",";
    j += "\"cat\":\"" + String(recCategoryToString(r.category)) + "\",";
    j += "\"title\":\"" + escapeJson(r.title) + "\",";
    j += "\"desc\":\"" + escapeJson(r.description) + "\",";
    j += "\"prio\":" + String(r.priority, 3) + ",";
    j += "\"dismissed\":" + String(r.dismissed ? "true" : "false") + ",";
    j += "\"acted\":" + String(r.acted ? "true" : "false") + ",";
    j += "\"exp\":" + String(r.expiry) + ",";
    j += "\"explain\":\"" + escapeJson(r.explanation) + "\",";
    j += "\"conf\":\"" + escapeJson(r.confidence) + "\",";
    j += "\"src\":\"" + escapeJson(r.sourceData) + "\",";
    j += "\"rel\":" + String(r.relevanceScore, 3);
    j += "}";
    return j;
}

Recommendation ExecutiveAssistant::deserializeRec(const String& json) const noexcept {
    Recommendation r;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    auto eF = [&](const char* key, float def) -> float {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && (json[end] == '-' || json[end] == '.' || (json[end] >= '0' && json[end] <= '9'))) end++;
        return (end == start) ? def : json.substring(start, end).toFloat();
    };
    auto eB = [&](const char* key, bool def) -> bool {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        return json.substring(start).startsWith("true");
    };
    r.id = eS("id");
    r.timestamp = static_cast<unsigned long>(eI("ts", 0));
    String catStr = eS("cat");
    r.category = recStringToCategory(catStr);
    r.title = eS("title");
    r.description = eS("desc");
    r.priority = eF("prio", 0.5f);
    r.dismissed = eB("dismissed", false);
    r.acted = eB("acted", false);
    r.expiry = static_cast<unsigned long>(eI("exp", 0));
    r.explanation = eS("explain");
    r.confidence = eS("conf");
    if (r.confidence.isEmpty()) r.confidence = "medium";
    r.sourceData = eS("src");
    r.relevanceScore = eF("rel", 0.0f);
    return r;
}
