#include "briefing_manager.h"
#include "json_helpers.h"
#include "memory_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "planner_manager.h"
#include "reminder_manager.h"
#include "personality_manager.h"
#include "context_manager.h"
#include "timeline_manager.h"
#include "conversation_manager.h"
#include "wifi_manager.h"

BriefingManager briefingManager;

BriefingManager::BriefingManager() noexcept
    : m_initialized(false)
    , m_briefingsDirty(false)
    , m_lastIdCounter(0)
    , m_lastGenerateTime(0)
    , m_summariesDirty(false) {
    m_records.reserve(kMaxRecords);
    m_summaries.reserve(kMaxSummaries);
}

BriefingManager::~BriefingManager() noexcept {
    if (m_briefingsDirty) saveBriefings();
    if (m_summariesDirty) saveSummaries();
}

bool BriefingManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(BRIEFING_PATH, StorageType::SPIFFS);
    storageManager.createDirectory("/cache", StorageType::SPIFFS);
    loadBriefings();
    loadSummaries();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void BriefingManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (m_briefingsDirty && (now - m_lastGenerateTime > 5000)) {
        m_lastGenerateTime = now;
        if (saveBriefings()) m_briefingsDirty = false;
    }
    if (m_summariesDirty && (now - m_lastGenerateTime > 5000)) {
        m_lastGenerateTime = now;
        if (saveSummaries()) m_summariesDirty = false;
    }
}

String BriefingManager::generateMorning() noexcept {
    String content = buildMorning();
    saveBriefingRecord("morning", content);
    return content;
}

String BriefingManager::generateEvening() noexcept {
    String content = buildEvening();
    saveBriefingRecord("evening", content);
    return content;
}

bool BriefingManager::isInitialized() const noexcept { return m_initialized; }

String BriefingManager::buildMorning() noexcept {
    String result;
    result.reserve(BRIEFING_MAX_LENGTH);
    result += "Good morning! ";
    if (personalityManager.isInitialized()) {
        result += "I'm " + personalityManager.getActiveProfile().name + " mode. ";
    }
    result += "Here's your briefing for today. ";
    if (goalManager.isInitialized()) {
        auto goals = goalManager.getActiveGoals();
        if (!goals.empty()) {
            result += "Goals: " + String(goals.size()) + " active. ";
        }
    }
    if (habitManager.isInitialized()) {
        auto due = habitManager.getDueHabits();
        if (!due.empty()) {
            result += "Habits due: " + String(due.size()) + ". ";
        }
    }
    if (reminderManager.isInitialized()) {
        std::vector<Reminder> reminders;
        size_t rCount = reminderManager.getReminders(reminders);
        if (rCount > 0) {
            result += "Reminders: " + String(rCount) + ". ";
        }
    }
    if (plannerManager.isInitialized()) {
        auto tasks = plannerManager.getTodaysTasks();
        if (!tasks.empty()) {
            result += "Today's tasks: " + String(tasks.size()) + ". ";
        }
    }
    if (contextManager.isInitialized()) {
        auto& ctx = contextManager.getContext();
        if (!ctx.currentProject.isEmpty()) {
            result += "Project: " + ctx.currentProject + ". ";
        }
    }
    if (storageManager.isHealthy()) {
        size_t total = storageManager.getTotalSpace();
        size_t free = storageManager.getFreeSpace();
        if (total > 0) {
            float pct = 100.0f * free / total;
            result += "Storage: " + String(static_cast<int>(pct)) + "% free. ";
        }
    }
    result += "Ready to assist.";
    return result;
}

String BriefingManager::buildEvening() noexcept {
    String result;
    result.reserve(BRIEFING_MAX_LENGTH);
    result += "Evening summary. ";
    auto today = timelineManager.getToday();
    result += "Today had " + String(today.size()) + " timeline events. ";
    size_t completed = 0;
    for (const auto& e : today) {
        if (e.category == "goal_completed" || e.category == "habit_completed" || e.category == "reminder_completed") completed++;
    }
    if (completed > 0) {
        result += "Completed " + String(completed) + " items. ";
    }
    if (memoryManager.isInitialized()) {
        auto ranked = memoryManager.getRankedMemories(3);
        if (!ranked.empty()) {
            result += "Top memories: ";
            for (size_t i = 0; i < ranked.size(); ++i) {
                if (i > 0) result += "; ";
                result += ranked[i].key;
            }
            result += ". ";
        }
    }
    result += "Have a good evening.";
    return result;
}

bool BriefingManager::saveBriefingRecord(const String& type, const String& content) noexcept {
    BriefingRecord rec;
    rec.id = generateId();
    rec.timestamp = millis();
    rec.type = type;
    rec.content = content;
    m_records.push_back(rec);
    while (m_records.size() > kMaxRecords) m_records.erase(m_records.begin());
    m_briefingsDirty = true;
    return true;
}

bool BriefingManager::saveBriefings() noexcept {
    String json; json.reserve(16384);
    json += "{\"version\":" + String(BRIEFING_FILE_VERSION) + ",\"records\":[";
    for (size_t i = 0; i < m_records.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":\"" + escapeJson(m_records[i].id) + "\",";
        json += "\"ts\":" + String(m_records[i].timestamp) + ",";
        json += "\"type\":\"" + escapeJson(m_records[i].type) + "\",";
        json += "\"content\":\"" + escapeJson(m_records[i].content) + "\"";
        json += "}";
    }
    json += "]}";
    String path = String(BRIEFING_PATH) + "/data.json";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_briefingsDirty = false; return true; }
    return false;
}

bool BriefingManager::loadBriefings() noexcept {
    String path = String(BRIEFING_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_records.clear();
    int pos = content.indexOf('[');
    if (pos < 0) return false;
    pos++;
    while (pos < (int)content.length()) {
        int bs = content.indexOf('{', pos);
        if (bs < 0) break;
        int be = content.indexOf('}', bs);
        if (be < 0) break;
        String obj = content.substring(bs, be + 1);
        auto ex = [&](const char* key) -> String {
            String s = String("\"") + key + "\":\"";
            int st = obj.indexOf(s);
            if (st < 0) return "";
            st += s.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        BriefingRecord r;
        r.id = ex("id");
        r.type = ex("type");
        r.content = ex("content");
        if (!r.id.isEmpty()) m_records.push_back(r);
        pos = be + 1;
    }
    return true;
}

String BriefingManager::generateId() noexcept {
    return ::generateId();
}

// ========================================================================
// DailySummary implementations (merged from DailySummaryManager)
// ========================================================================

bool BriefingManager::generateTodaySummary() noexcept {
    if (!m_initialized) return false;
    if (!getTodaySummary().id.isEmpty()) return true;
    DailySummary summary;
    summary.id = generateSummaryId();
    summary.date = getTodayDate();
    summary.timestamp = millis();
    summary.content.reserve(SUMMARY_MAX_LENGTH);
    summary.content += "Daily summary for " + summary.date + ". ";
    if (memoryManager.isInitialized()) {
        auto ranked = memoryManager.getRankedMemories(3);
        if (!ranked.empty()) {
            summary.content += "Top memories: ";
            for (size_t i = 0; i < ranked.size(); ++i) {
                if (i > 0) summary.content += "; ";
                summary.content += ranked[i].key;
            }
            summary.content += ". ";
        }
        summary.memoryCount = ranked.size();
    }
    if (reminderManager.isInitialized()) {
        std::vector<Reminder> reminders;
        summary.reminderCount = reminderManager.getReminders(reminders);
    }
    if (conversationManager.isInitialized()) {
        summary.conversationCount = conversationManager.getHistory().size();
    }
    m_summaries.push_back(summary);
    m_summariesDirty = true;
    return true;
}

DailySummary BriefingManager::getTodaySummary() const noexcept {
    String today = getTodayDate();
    for (const auto& s : m_summaries) {
        if (s.date == today && !s.archived) return s;
    }
    return DailySummary();
}

const std::vector<DailySummary>& BriefingManager::getAllSummaries() const noexcept {
    return m_summaries;
}

bool BriefingManager::deleteSummary(const String& summaryId) noexcept {
    size_t idx = findSummary(summaryId);
    if (idx >= m_summaries.size()) return false;
    m_summaries.erase(m_summaries.begin() + idx);
    m_summariesDirty = true;
    return true;
}

bool BriefingManager::saveSummaries() noexcept {
    if (!m_initialized) return false;
    String json; json.reserve(8192);
    json += "[";
    for (size_t i = 0; i < m_summaries.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":\"" + escapeJson(m_summaries[i].id) + "\",";
        json += "\"date\":\"" + escapeJson(m_summaries[i].date) + "\",";
        json += "\"ts\":" + String(m_summaries[i].timestamp) + ",";
        json += "\"content\":\"" + escapeJson(m_summaries[i].content) + "\",";
        json += "\"reminders\":" + String(m_summaries[i].reminderCount) + ",";
        json += "\"memories\":" + String(m_summaries[i].memoryCount) + ",";
        json += "\"conversations\":" + String(m_summaries[i].conversationCount) + ",";
        json += "\"archived\":" + String(m_summaries[i].archived ? "true" : "false") + ",";
        json += "\"favorite\":" + String(m_summaries[i].favorite ? "true" : "false");
        json += "}";
    }
    json += "]";
    StorageStatus st = storageManager.writeFile(kSummariesPath, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_summariesDirty = false; return true; }
    return false;
}

bool BriefingManager::loadSummaries() noexcept {
    if (!storageManager.fileExists(kSummariesPath, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(kSummariesPath, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_summaries.clear();
    int pos = 0;
    pos = content.indexOf('[');
    if (pos < 0) return false;
    pos++;
    while (pos < (int)content.length()) {
        int bs = content.indexOf('{', pos);
        if (bs < 0) break;
        int be = content.indexOf('}', bs);
        if (be < 0) break;
        String obj = content.substring(bs, be + 1);
        auto ex = [&](const char* key) -> String {
            String s = String("\"") + key + "\":\"";
            int st = obj.indexOf(s);
            if (st < 0) return "";
            st += s.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        auto exInt = [&](const char* key) -> size_t {
            String s = String("\"") + key + "\":";
            int st = obj.indexOf(s);
            if (st < 0) return 0;
            st += s.length();
            int en = obj.indexOf(',', st);
            if (en < 0) en = obj.indexOf('}', st);
            return (en < 0) ? obj.substring(st).toInt() : obj.substring(st, en).toInt();
        };
        auto exBool = [&](const char* key) -> bool {
            String s = String("\"") + key + "\":";
            int st = obj.indexOf(s);
            if (st < 0) return false;
            st += s.length();
            return obj.substring(st).startsWith("true");
        };
        DailySummary s;
        s.id = ex("id");
        s.date = ex("date");
        s.timestamp = exInt("ts");
        s.content = ex("content");
        s.reminderCount = exInt("reminders");
        s.memoryCount = exInt("memories");
        s.conversationCount = exInt("conversations");
        s.archived = exBool("archived");
        s.favorite = exBool("favorite");
        if (!s.id.isEmpty()) m_summaries.push_back(s);
        pos = be + 1;
    }
    return true;
}

String BriefingManager::generateSummaryId() noexcept {
    return ::generateId();
}

String BriefingManager::getTodayDate() const noexcept {
    return "2026-07-29";
}

size_t BriefingManager::findSummary(const String& id) const noexcept {
    for (size_t i = 0; i < m_summaries.size(); ++i) {
        if (m_summaries[i].id == id) return i;
    }
    return m_summaries.size();
}
