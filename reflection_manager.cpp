#include "reflection_manager.h"
#include "json_helpers.h"
#include "memory_manager.h"
#include "knowledge_graph_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "briefing_manager.h"

ReflectionManager reflectionManager;

ReflectionManager::ReflectionManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastRunDay(0), m_lastIdCounter(0) {
    m_history.reserve(kMaxHistory);
}

ReflectionManager::~ReflectionManager() noexcept { if (m_dirty) save(); }

bool ReflectionManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u reflections)", m_history.size());
    return true;
}

void ReflectionManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    unsigned long now = millis();

    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }

    // Auto-reflect once per day
    unsigned long today = now / 86400000UL;
    if (today != m_lastRunDay) {
        m_lastRunDay = today;
        runReflection();
    }
}

ReflectionRecord ReflectionManager::runReflection() noexcept {
    if (!m_initialized) return ReflectionRecord();

    Logger::info(kLogCategory, "Running nightly reflection...");
    ReflectionRecord rec;
    rec.id = generateId();
    rec.date = generateDate();
    rec.timestamp = millis();

    // 1. Remove expired temporary memories
    rec.temporariesRemoved = memoryManager.removeExpired();

    // 2. Merge duplicate memories
    rec.duplicatesMerged = memoryManager.mergeDuplicates();

    // 3. Update importance scores
    memoryManager.updateImportanceScores();

    // 4. Auto-link knowledge graph
    rec.graphLinksAdded = knowledgeGraphManager.autoLink();

    // 5. Calculate productivity from goals
    size_t completedGoals = 0;
    size_t totalGoals = goalManager.goalCount();
    auto allGoals = goalManager.getAllGoals();
    for (const auto& g : allGoals) { if (g.completed) completedGoals++; }
    rec.productivityScore = (totalGoals > 0) ? (static_cast<float>(completedGoals) / totalGoals) * 100.0f : 0.0f;

    // 6. Generate daily summary
    briefingManager.generateTodaySummary();

    rec.summary = "Reflection complete. ";
    rec.summary += "Cleaned " + String(rec.temporariesRemoved) + " expired memories, ";
    rec.summary += "merged " + String(rec.duplicatesMerged) + " duplicates, ";
    rec.summary += "linked " + String(rec.graphLinksAdded) + " graph nodes. ";
    rec.summary += "Productivity: " + String(rec.productivityScore, 0) + "%";

    m_history.push_back(rec);
    while (m_history.size() > kMaxHistory) m_history.erase(m_history.begin());
    m_dirty = true;

    Logger::info(kLogCategory, "Reflection complete: %s", rec.summary.c_str());
    return rec;
}

ReflectionRecord ReflectionManager::getLatestReflection() const noexcept {
    return m_history.empty() ? ReflectionRecord() : m_history.back();
}

const std::vector<ReflectionRecord>& ReflectionManager::getHistory() const noexcept {
    return m_history;
}

size_t ReflectionManager::reflectionCount() const noexcept { return m_history.size(); }
bool ReflectionManager::isInitialized() const noexcept { return m_initialized; }

bool ReflectionManager::save() noexcept {
    String json; json.reserve(4096);
    json += "{\"reflections\":[";
    for (size_t i = 0; i < m_history.size(); ++i) {
        if (i > 0) json += ",";
        const auto& r = m_history[i];
        json += "{\"id\":\"" + escapeJson(r.id) + "\",";
        json += "\"date\":\"" + escapeJson(r.date) + "\",";
        json += "\"productivity\":" + String(r.productivityScore, 1) + ",";
        json += "\"summary\":\"" + escapeJson(r.summary) + "\"}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(REFLECTION_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool ReflectionManager::load() noexcept {
    if (!storageManager.fileExists(REFLECTION_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(REFLECTION_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_history.clear();
    int pos = 0;
    while (true) {
        int s = content.indexOf('{', pos); if (s < 0) break;
        int e = content.indexOf('}', s); if (e < 0) break;
        String obj = content.substring(s, e + 1);
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\""; int st = obj.indexOf(q);
            if (st < 0) { q = String("\"") + k + "\":"; st = obj.indexOf(q); if (st < 0) return ""; st += q.length(); int en = st; while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++; return obj.substring(st, en); }
            st += q.length(); int en = obj.indexOf('"', st); return (en < 0) ? "" : obj.substring(st, en);
        };
        ReflectionRecord r; r.id = ext("id"); r.date = ext("date");
        if (!r.id.isEmpty()) { m_history.push_back(r); }
        pos = e + 1;
    }
    return true;
}

String ReflectionManager::generateDate() const noexcept {
    unsigned long days = millis() / 86400000UL;
    char buf[16];
    snprintf(buf, sizeof(buf), "%04lu-%02lu-%02lu", 2026UL, 1UL + (days % 12), 1UL + (days % 28));
    return String(buf);
}

String ReflectionManager::generateId() noexcept {
    return ::generateId();
}
