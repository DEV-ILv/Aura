#include "timeline_manager.h"
#include "json_helpers.h"

TimelineManager timelineManager;

TimelineManager::TimelineManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0), m_lastBatchSave(0) {
}

TimelineManager::~TimelineManager() noexcept {
    if (m_dirty) save();
}

bool TimelineManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory("/context", StorageType::SPIFFS);
    storageManager.createDirectory(TIMELINE_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u entries)", m_entries.size());
    return true;
}

void TimelineManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (m_dirty && (now - m_lastBatchSave > 10000)) {
        m_lastBatchSave = now;
        if (save()) m_dirty = false;
    }
}

String TimelineManager::addEntry(TimelineCategory category, const String& summary,
                                  uint8_t importance, const String& linkedMemories,
                                  const String& linkedGoals, const String& linkedConversations,
                                  const String& linkedGraphNodes) noexcept {
    if (!m_initialized) return "";
    TimelineEntry e;
    e.id = generateId();
    e.timestamp = millis();
    e.dateKey = computeDateKey(e.timestamp);
    e.category = categoryToString(category);
    e.summary = summary;
    e.importance = importance;
    e.linkedMemoryIds = linkedMemories;
    e.linkedGoalIds = linkedGoals;
    e.linkedConversationIds = linkedConversations;
    e.linkedGraphNodeIds = linkedGraphNodes;
    m_entries.push_back(e);
    m_dirty = true;
    trimToMax();
    return e.id;
}

std::vector<TimelineEntry> TimelineManager::getToday() const noexcept {
    unsigned long today = computeDateKey(millis());
    return getRange(today, today);
}

std::vector<TimelineEntry> TimelineManager::getYesterday() const noexcept {
    unsigned long today = computeDateKey(millis());
    unsigned long yesterday = today - 1;
    return getRange(yesterday, yesterday);
}

std::vector<TimelineEntry> TimelineManager::getThisWeek() const noexcept {
    unsigned long today = computeDateKey(millis());
    unsigned long weekStart = today - 7;
    return getRange(weekStart, today);
}

std::vector<TimelineEntry> TimelineManager::getThisMonth() const noexcept {
    unsigned long today = computeDateKey(millis());
    unsigned long monthStart = today - 30;
    return getRange(monthStart, today);
}

std::vector<TimelineEntry> TimelineManager::getRange(unsigned long startDate, unsigned long endDate) const noexcept {
    std::vector<TimelineEntry> results;
    for (const auto& e : m_entries) {
        if (e.dateKey >= startDate && e.dateKey <= endDate) {
            results.push_back(e);
        }
    }
    return results;
}

std::vector<TimelineEntry> TimelineManager::search(const String& query) const noexcept {
    std::vector<TimelineEntry> results;
    if (query.isEmpty()) return results;
    String lq = query; lq.toLowerCase();
    for (const auto& e : m_entries) {
        String ls = e.summary; ls.toLowerCase();
        String lc = e.category; lc.toLowerCase();
        if (ls.indexOf(lq) >= 0 || lc.indexOf(lq) >= 0) {
            results.push_back(e);
        }
    }
    return results;
}

size_t TimelineManager::entryCount() const noexcept { return m_entries.size(); }
bool TimelineManager::isInitialized() const noexcept { return m_initialized; }

bool TimelineManager::save() noexcept {
    String path = String(TIMELINE_PATH) + "/data.json";
    String json;
    // Reserve proportional to actual entry count (~300B avg) so the payload
    // reaches its final size with few reallocations; reserve() is a hint and
    // failure falls back to normal String growth.
    json.reserve(m_entries.size() * 300U + 64U);
    json += "{\"version\":" + String(TIMELINE_FILE_VERSION) + ",\"entries\":[";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeEntry(m_entries[i]);
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool TimelineManager::load() noexcept {
    String path = String(TIMELINE_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_entries.clear();
    int entriesStart = content.indexOf("\"entries\":[");
    if (entriesStart < 0) return false;
    int pos = content.indexOf('[', entriesStart) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        TimelineEntry e = deserializeEntry(obj);
        if (!e.id.isEmpty()) m_entries.push_back(e);
        pos = braceEnd + 1;
    }
    return true;
}

String TimelineManager::generateId() noexcept {
    return ::generateId();
}

unsigned long TimelineManager::computeDateKey(unsigned long ts) const noexcept {
    return ts / 86400000UL;
}

const char* TimelineManager::categoryToString(TimelineCategory c) const noexcept {
    switch (c) {
        case TimelineCategory::GOAL_CREATED: return "goal_created";
        case TimelineCategory::GOAL_COMPLETED: return "goal_completed";
        case TimelineCategory::REMINDER_COMPLETED: return "reminder_completed";
        case TimelineCategory::CONVERSATION: return "conversation";
        case TimelineCategory::MEMORY_SAVED: return "memory_saved";
        case TimelineCategory::HABIT_COMPLETED: return "habit_completed";
        case TimelineCategory::PROJECT_STARTED: return "project_started";
        case TimelineCategory::PROJECT_FINISHED: return "project_finished";
        case TimelineCategory::PLUGIN_INSTALLED: return "plugin_installed";
        case TimelineCategory::OTA_UPDATE: return "ota_update";
        case TimelineCategory::STUDY_SESSION: return "study_session";
        default: return "custom";
    }
}

TimelineCategory TimelineManager::stringToCategory(const String& s) const noexcept {
    if (s == "goal_created") return TimelineCategory::GOAL_CREATED;
    if (s == "goal_completed") return TimelineCategory::GOAL_COMPLETED;
    if (s == "reminder_completed") return TimelineCategory::REMINDER_COMPLETED;
    if (s == "conversation") return TimelineCategory::CONVERSATION;
    if (s == "memory_saved") return TimelineCategory::MEMORY_SAVED;
    if (s == "habit_completed") return TimelineCategory::HABIT_COMPLETED;
    if (s == "project_started") return TimelineCategory::PROJECT_STARTED;
    if (s == "project_finished") return TimelineCategory::PROJECT_FINISHED;
    if (s == "plugin_installed") return TimelineCategory::PLUGIN_INSTALLED;
    if (s == "ota_update") return TimelineCategory::OTA_UPDATE;
    if (s == "study_session") return TimelineCategory::STUDY_SESSION;
    return TimelineCategory::CUSTOM;
}

void TimelineManager::trimToMax() noexcept {
    while (m_entries.size() > kMaxEntries) {
        m_entries.erase(m_entries.begin());
    }
}

String TimelineManager::serializeEntry(const TimelineEntry& e) const noexcept {
    String j; j.reserve(256);
    j += "{";
    j += "\"id\":\"" + escapeJson(e.id) + "\",";
    j += "\"ts\":" + String(e.timestamp) + ",";
    j += "\"date\":" + String(e.dateKey) + ",";
    j += "\"cat\":\"" + escapeJson(e.category) + "\",";
    j += "\"sum\":\"" + escapeJson(e.summary) + "\",";
    j += "\"imp\":" + String(e.importance) + ",";
    j += "\"mem\":\"" + escapeJson(e.linkedMemoryIds) + "\",";
    j += "\"goal\":\"" + escapeJson(e.linkedGoalIds) + "\",";
    j += "\"conv\":\"" + escapeJson(e.linkedConversationIds) + "\",";
    j += "\"graph\":\"" + escapeJson(e.linkedGraphNodeIds) + "\"";
    j += "}";
    return j;
}

TimelineEntry TimelineManager::deserializeEntry(const String& json) const noexcept {
    TimelineEntry e;
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
    e.id = eS("id");
    e.timestamp = static_cast<unsigned long>(eI("ts", 0));
    e.dateKey = static_cast<unsigned long>(eI("date", 0));
    e.category = eS("cat");
    e.summary = eS("sum");
    e.importance = static_cast<uint8_t>(eI("imp", 0));
    e.linkedMemoryIds = eS("mem");
    e.linkedGoalIds = eS("goal");
    e.linkedConversationIds = eS("conv");
    e.linkedGraphNodeIds = eS("graph");
    return e;
}
