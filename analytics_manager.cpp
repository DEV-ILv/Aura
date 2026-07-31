#include "analytics_manager.h"
#include "json_helpers.h"
#include <algorithm>

AnalyticsManager analyticsManager;

AnalyticsManager::AnalyticsManager() noexcept
    : m_initialized(false), m_lastSaveTime(0), m_lastAggregateTime(0) {}

AnalyticsManager::~AnalyticsManager() noexcept = default;

bool AnalyticsManager::initialize() noexcept {
    if (m_initialized) return true;
    if (!storageManager.isInitialized()) return false;
    load();
    m_initialized = true;
    m_lastSaveTime = millis();
    m_lastAggregateTime = millis();
    LOG_INFO(kLogCategory, "AnalyticsManager initialized (%u records)", static_cast<unsigned int>(m_records.size()));
    return true;
}

void AnalyticsManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastAggregateTime >= 300000) {
        m_lastAggregateTime = now;
        aggregateDaily();
    }
    if (now - m_lastSaveTime >= kSaveIntervalMs) {
        m_lastSaveTime = now;
        save();
    }
}

void AnalyticsManager::record(const String& category, const String& metric, float value, const String& unit, const String& tags) noexcept {
    if (!m_initialized) return;
    if (m_records.size() >= kMaxRecords) {
        m_records.erase(m_records.begin());
    }
    AnalyticsRecord rec;
    rec.id = m_records.size() + 1;
    rec.timestamp = millis();
    rec.category = category;
    rec.metric = metric;
    rec.value = value;
    rec.unit = unit;
    rec.tags = tags;
    m_records.push_back(rec);

    if (eventBus.isInitialized()) {
        String data = "{\"category\":\"" + category + "\",\"metric\":\"" + metric + "\",\"value\":" + String(value) + "}";
        eventBus.publish(EventType::ANALYTICS_RECORD_ADDED, "AnalyticsManager", data);
    }
}

std::vector<AnalyticsRecord> AnalyticsManager::getRecords(const String& category, const String& metric, size_t limit) noexcept {
    std::vector<AnalyticsRecord> filtered;
    for (const auto& rec : m_records) {
        if (!category.isEmpty() && rec.category != category) continue;
        if (!metric.isEmpty() && rec.metric != metric) continue;
        filtered.push_back(rec);
    }
    if (filtered.size() > limit) {
        filtered.erase(filtered.begin(), filtered.begin() + (filtered.size() - limit));
    }
    return filtered;
}

AnalyticsTrend AnalyticsManager::getTrend(const String& category, const String& metric) noexcept {
    auto recs = getRecords(category, metric, 500);
    AnalyticsTrend trend;
    trend.metric = metric;
    trend.average = 0;
    trend.min = 999999;
    trend.max = -999999;
    trend.dataPoints = recs.size();
    trend.increasing = false;

    if (recs.empty()) return trend;

    float sum = 0;
    for (size_t i = 0; i < recs.size(); ++i) {
        sum += recs[i].value;
        if (recs[i].value < trend.min) trend.min = recs[i].value;
        if (recs[i].value > trend.max) trend.max = recs[i].value;
    }
    trend.average = sum / recs.size();

    if (recs.size() >= 3) {
        size_t half = recs.size() / 2;
        float firstHalf = 0, secondHalf = 0;
        for (size_t i = 0; i < half; ++i) firstHalf += recs[i].value;
        for (size_t i = half; i < recs.size(); ++i) secondHalf += recs[i].value;
        firstHalf /= half;
        secondHalf /= (recs.size() - half);
        trend.slope = secondHalf - firstHalf;
        trend.increasing = trend.slope > 0;
    }

    return trend;
}

AnalyticsSummary AnalyticsManager::getDailySummary(unsigned long dateKey) noexcept {
    AnalyticsSummary s;
    s.dateKey = dateKey;
    s.studyHours = 0;
    s.remindersCompleted = 0;
    s.tasksCompleted = 0;
    s.conversationsHad = 0;
    s.knowledgeFactsAdded = 0;
    s.projectsWorkedOn = 0;
    s.systemUptimeHours = 0;
    s.productivityScore = 0;

    for (const auto& rec : m_records) {
        unsigned long recDate = rec.timestamp / 86400000UL;
        if (recDate != dateKey) continue;

        if (rec.category == "study" && rec.metric == "minutes") s.studyHours += rec.value / 60.0f;
        else if (rec.category == "reminder" && rec.metric == "completed") s.remindersCompleted += (size_t)rec.value;
        else if (rec.category == "task" && rec.metric == "completed") s.tasksCompleted += (size_t)rec.value;
        else if (rec.category == "conversation" && rec.metric == "count") s.conversationsHad += (size_t)rec.value;
        else if (rec.category == "knowledge" && rec.metric == "added") s.knowledgeFactsAdded += (size_t)rec.value;
        else if (rec.category == "project" && rec.metric == "worked") s.projectsWorkedOn += (size_t)rec.value;
        else if (rec.category == "system" && rec.metric == "uptime") s.systemUptimeHours = rec.value / 3600.0f;
    }

    float total = s.studyHours * 10 + s.remindersCompleted * 5 + s.tasksCompleted * 5 + s.knowledgeFactsAdded * 3;
    if (total > 100) total = 100;
    s.productivityScore = total;

    return s;
}

AnalyticsSummary AnalyticsManager::getWeeklySummary(unsigned long weekKey) noexcept {
    AnalyticsSummary s;
    s.dateKey = weekKey;
    for (unsigned long d = 0; d < 7; ++d) {
        auto daily = getDailySummary(weekKey + d);
        s.studyHours += daily.studyHours;
        s.remindersCompleted += daily.remindersCompleted;
        s.tasksCompleted += daily.tasksCompleted;
        s.conversationsHad += daily.conversationsHad;
        s.knowledgeFactsAdded += daily.knowledgeFactsAdded;
        s.projectsWorkedOn += daily.projectsWorkedOn;
        s.systemUptimeHours += daily.systemUptimeHours;
    }
    s.productivityScore = s.productivityScore / 7.0f;
    return s;
}

AnalyticsSummary AnalyticsManager::getMonthlySummary(unsigned long monthKey) noexcept {
    AnalyticsSummary s;
    s.dateKey = monthKey;
    for (unsigned long d = 0; d < 30; ++d) {
        auto daily = getDailySummary(monthKey + d);
        s.studyHours += daily.studyHours;
        s.remindersCompleted += daily.remindersCompleted;
        s.tasksCompleted += daily.tasksCompleted;
        s.conversationsHad += daily.conversationsHad;
        s.knowledgeFactsAdded += daily.knowledgeFactsAdded;
        s.projectsWorkedOn += daily.projectsWorkedOn;
        s.systemUptimeHours += daily.systemUptimeHours;
    }
    s.productivityScore = s.productivityScore / 30.0f;
    return s;
}

float AnalyticsManager::getTotalStudyMinutes() noexcept {
    float total = 0;
    for (const auto& rec : m_records) {
        if (rec.category == "study" && rec.metric == "minutes") total += rec.value;
    }
    return total;
}

float AnalyticsManager::getReminderCompletionRate() noexcept {
    size_t completed = 0, total = 0;
    for (const auto& rec : m_records) {
        if (rec.category == "reminder" && rec.metric == "completed") completed += (size_t)rec.value;
        if (rec.category == "reminder" && rec.metric == "total") total += (size_t)rec.value;
    }
    return total > 0 ? (float)completed / (float)total * 100.0f : 0;
}

size_t AnalyticsManager::getTotalConversations() noexcept {
    size_t total = 0;
    for (const auto& rec : m_records) {
        if (rec.category == "conversation" && rec.metric == "count") total += (size_t)rec.value;
    }
    return total;
}

size_t AnalyticsManager::getTotalKnowledgeFacts() noexcept {
    size_t total = 0;
    for (const auto& rec : m_records) {
        if (rec.category == "knowledge" && rec.metric == "added") total += (size_t)rec.value;
    }
    return total;
}

bool AnalyticsManager::isInitialized() const noexcept {
    return m_initialized;
}

bool AnalyticsManager::save() noexcept {
    if (!m_initialized) return false;
    String path = String(CACHE_FOLDER) + "/analytics.json";
    String json; json.reserve(4096);
    json += "{\"records\":[";
    for (size_t i = 0; i < m_records.size(); ++i) {
        if (i > 0) json += ",";
        const auto& r = m_records[i];
        json += "{";
        json += "\"id\":" + String(r.id) + ",";
        json += "\"ts\":" + String(r.timestamp) + ",";
        json += "\"cat\":\"" + escapeJson(r.category) + "\",";
        json += "\"met\":\"" + escapeJson(r.metric) + "\",";
        json += "\"val\":" + String(r.value, 4) + ",";
        json += "\"unit\":\"" + escapeJson(r.unit) + "\",";
        json += "\"tags\":\"" + escapeJson(r.tags) + "\"";
        json += "}";
    }
    json += "]}";
    return storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS) == StorageStatus::SUCCESS;
}

bool AnalyticsManager::load() noexcept {
    String path = String(CACHE_FOLDER) + "/analytics.json";
    String content;
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return true;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return true;
    m_records.clear();

    // Parse JSON manually (hand-rolled, consistent with other managers)
    int pos = content.indexOf("\"records\":[");
    if (pos < 0) return true;
    pos = content.indexOf('[', pos) + 1;

    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        pos = braceEnd + 1;

        auto findStr = [&](const char* key) -> String {
            String s = String("\"") + key + "\":\"";
            int st = obj.indexOf(s);
            if (st < 0) return "";
            st += s.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        auto findNum = [&](const char* key, double def) -> double {
            String s = String("\"") + key + "\":";
            int st = obj.indexOf(s);
            if (st < 0) return def;
            st += s.length();
            int en = st;
            while (en < (int)obj.length() && (obj[en] == '-' || obj[en] == '.' || (obj[en] >= '0' && obj[en] <= '9'))) en++;
            return (en == st) ? def : obj.substring(st, en).toDouble();
        };

        AnalyticsRecord r;
        r.id = static_cast<unsigned long>(findNum("id", 0));
        r.timestamp = static_cast<unsigned long>(findNum("ts", 0));
        r.category = findStr("cat");
        r.metric = findStr("met");
        r.value = static_cast<float>(findNum("val", 0));
        r.unit = findStr("unit");
        r.tags = findStr("tags");
        m_records.push_back(r);
    }

    LOG_INFO(kLogCategory, "Loaded %u analytics records", static_cast<unsigned int>(m_records.size()));
    return true;
}

void AnalyticsManager::aggregateDaily() noexcept {
    // Daily aggregation is computed on-the-fly from raw records.
    // No separate aggregation storage needed.
}

unsigned long AnalyticsManager::dateKeyFromTimestamp(unsigned long ts) noexcept {
    return ts / 86400000UL;
}
