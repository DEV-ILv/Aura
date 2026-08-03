#include "learning_manager.h"
#include "json_helpers.h"
#include <algorithm>

LearningManager learningManager;

LearningManager::LearningManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0), m_lastAnalysisTime(0) {
}

LearningManager::~LearningManager() noexcept {
    if (m_dirty) save();
}

bool LearningManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(LEARNING_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u obs, %u patterns)", m_observations.size(), m_patterns.size());
    return true;
}

void LearningManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastAnalysisTime >= kObserveIntervalMs) {
        m_lastAnalysisTime = now;
        analyzePatterns();
    }
    if (m_dirty && save()) m_dirty = false;
}

String LearningManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

void LearningManager::observe(const String& data, const String& category, LearningPatternType type) noexcept {
    if (!m_initialized) return;
    LearningObservation obs;
    obs.id = generateId();
    obs.timestamp = millis();
    obs.data = data;
    obs.category = category;
    obs.patternType = type;
    m_observations.push_back(obs);
    trimObservations();
    m_dirty = true;
}

void LearningManager::observeSchedule(unsigned long timestamp, const String& activity) noexcept {
    observe(String(timestamp) + "|" + activity, "schedule", LearningPatternType::SCHEDULE_REPEAT);
}

void LearningManager::observeCommand(const String& command) noexcept {
    observe(command, "command", LearningPatternType::FREQUENT_COMMAND);
}

void LearningManager::observeRoutine(const String& routineName, unsigned long timeOfDay) noexcept {
    observe(String(timeOfDay) + "|" + routineName, "routine", LearningPatternType::DAILY_ROUTINE);
}

void LearningManager::observeSession(const String& sessionType, unsigned long duration) noexcept {
    observe(sessionType + "|" + String(duration), "session", 
            (sessionType == "coding") ? LearningPatternType::CODING_SESSION :
            (sessionType == "study") ? LearningPatternType::STUDY_HABIT :
            LearningPatternType::CUSTOM);
}

std::vector<LearnedPattern> LearningManager::detectPatterns() noexcept {
    analyzePatterns();
    return m_patterns;
}

std::vector<LearnedPattern> LearningManager::getActivePatterns(size_t minConfidence) const noexcept {
    std::vector<LearnedPattern> active;
    for (const auto& p : m_patterns) {
        if (static_cast<size_t>(p.confidence * 100) >= minConfidence) {
            active.push_back(p);
        }
    }
    return active;
}

String LearningManager::generateSuggestion(const LearnedPattern& pattern) const noexcept {
    if (pattern.actionable) return pattern.suggestion;
    return "";
}

std::vector<LearningObservation> LearningManager::getRecentObservations(size_t count) const noexcept {
    std::vector<LearningObservation> recent;
    size_t start = (m_observations.size() > count) ? m_observations.size() - count : 0;
    for (size_t i = start; i < m_observations.size(); ++i) {
        recent.push_back(m_observations[i]);
    }
    return recent;
}

std::vector<LearnedPattern> LearningManager::getPatternsByType(LearningPatternType type) const noexcept {
    std::vector<LearnedPattern> results;
    for (const auto& p : m_patterns) {
        if (p.type == type) results.push_back(p);
    }
    return results;
}

String LearningManager::getPatternsJson() const noexcept {
    String json; json.reserve(4096);
    json += "{\"patterns\":[";
    bool first = true;
    for (const auto& p : m_patterns) {
        if (!first) json += ",";
        first = false;
        json += serializePattern(p);
    }
    json += "],\"observations\":[";
    first = true;
    size_t count = (m_observations.size() > 20) ? 20 : m_observations.size();
    for (size_t i = m_observations.size() - count; i < m_observations.size(); ++i) {
        if (!first) json += ",";
        first = false;
        json += serializeObservation(m_observations[i]);
    }
    json += "]}";
    return json;
}

bool LearningManager::save() noexcept {
    String path = String(LEARNING_PATH) + "/data.json";
    String json; json.reserve(16384);
    json += "{\"version\":1,\"observations\":[";
    for (size_t i = 0; i < m_observations.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeObservation(m_observations[i]);
    }
    json += "],\"patterns\":[";
    for (size_t i = 0; i < m_patterns.size(); ++i) {
        if (i > 0) json += ",";
        json += serializePattern(m_patterns[i]);
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    return (st == StorageStatus::SUCCESS);
}

bool LearningManager::load() noexcept {
    String path = String(LEARNING_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    
    m_observations.clear();
    m_patterns.clear();
    
    // Parse observations
    int obsStart = content.indexOf("\"observations\":[");
    if (obsStart >= 0) {
        int pos = content.indexOf('[', obsStart) + 1;
        while (pos < (int)content.length()) {
            int braceStart = content.indexOf('{', pos);
            if (braceStart < 0) break;
            int braceEnd = content.indexOf('}', braceStart);
            if (braceEnd < 0) break;
            String obj = content.substring(braceStart, braceEnd + 1);
            LearningObservation obs = deserializeObservation(obj);
            if (!obs.id.isEmpty()) m_observations.push_back(obs);
            pos = braceEnd + 1;
        }
    }
    
    // Parse patterns
    int patStart = content.indexOf("\"patterns\":[");
    if (patStart >= 0) {
        int pos = content.indexOf('[', patStart) + 1;
        while (pos < (int)content.length()) {
            int braceStart = content.indexOf('{', pos);
            if (braceStart < 0) break;
            int braceEnd = content.indexOf('}', braceStart);
            if (braceEnd < 0) break;
            String obj = content.substring(braceStart, braceEnd + 1);
            LearnedPattern pat = deserializePattern(obj);
            if (!pat.id.isEmpty()) m_patterns.push_back(pat);
            pos = braceEnd + 1;
        }
    }
    
    return true;
}

bool LearningManager::isInitialized() const noexcept { return m_initialized; }

void LearningManager::trimObservations() noexcept {
    while (m_observations.size() > kMaxObservations) {
        m_observations.erase(m_observations.begin());
    }
}

void LearningManager::analyzePatterns() noexcept {
    if (m_observations.size() < 3) return;
    
    // Simple frequency analysis by data content
    std::vector<std::pair<String, size_t>> freq;
    for (const auto& obs : m_observations) {
        auto it = std::find_if(freq.begin(), freq.end(),
            [&](const std::pair<String, size_t>& p) { return p.first == obs.data; });
        if (it != freq.end()) {
            it->second++;
        } else {
            freq.push_back({obs.data, 1});
        }
    }
    
    for (const auto& f : freq) {
        float confidence = static_cast<float>(f.second) / m_observations.size();
        if (confidence < kMinConfidence) continue;
        
        // Check if pattern already exists
        auto existing = std::find_if(m_patterns.begin(), m_patterns.end(),
            [&](const LearnedPattern& p) { return p.name == f.first; });
        
        if (existing != m_patterns.end()) {
            existing->confidence = confidence;
            existing->lastObserved = millis();
            existing->occurrenceCount = f.second;
        } else {
            LearnedPattern p;
            p.id = generateId();
            p.name = f.first;
            p.type = LearningPatternType::CUSTOM;
            p.confidence = confidence;
            p.firstObserved = millis();
            p.lastObserved = millis();
            p.occurrenceCount = f.second;
            p.actionable = (confidence > 0.5f);
            p.suggestion = generateSuggestion(p);
            m_patterns.push_back(p);
        }
    }
    
    // Trim patterns
    while (m_patterns.size() > kMaxPatterns) {
        auto minIt = std::min_element(m_patterns.begin(), m_patterns.end(),
            [](const LearnedPattern& a, const LearnedPattern& b) { return a.confidence < b.confidence; });
        if (minIt != m_patterns.end()) m_patterns.erase(minIt);
    }
    
    m_dirty = true;
}

const char* LearningManager::patternTypeToString(LearningPatternType t) const noexcept {
    switch (t) {
        case LearningPatternType::SCHEDULE_REPEAT: return "schedule_repeat";
        case LearningPatternType::FREQUENT_COMMAND: return "frequent_command";
        case LearningPatternType::DAILY_ROUTINE: return "daily_routine";
        case LearningPatternType::STUDY_HABIT: return "study_habit";
        case LearningPatternType::SLEEP_PATTERN: return "sleep_pattern";
        case LearningPatternType::CODING_SESSION: return "coding_session";
        case LearningPatternType::PROJECT_PREFERENCE: return "project_preference";
        case LearningPatternType::LEARNING_TOPIC: return "learning_topic";
        default: return "custom";
    }
}

LearningPatternType LearningManager::stringToPatternType(const String& s) const noexcept {
    if (s == "schedule_repeat") return LearningPatternType::SCHEDULE_REPEAT;
    if (s == "frequent_command") return LearningPatternType::FREQUENT_COMMAND;
    if (s == "daily_routine") return LearningPatternType::DAILY_ROUTINE;
    if (s == "study_habit") return LearningPatternType::STUDY_HABIT;
    if (s == "sleep_pattern") return LearningPatternType::SLEEP_PATTERN;
    if (s == "coding_session") return LearningPatternType::CODING_SESSION;
    if (s == "project_preference") return LearningPatternType::PROJECT_PREFERENCE;
    if (s == "learning_topic") return LearningPatternType::LEARNING_TOPIC;
    return LearningPatternType::CUSTOM;
}

String LearningManager::serializeObservation(const LearningObservation& obs) const noexcept {
    String j; j.reserve(128);
    j += "{";
    j += "\"id\":\"" + escapeJson(obs.id) + "\",";
    j += "\"ts\":" + String(obs.timestamp) + ",";
    j += "\"data\":\"" + escapeJson(obs.data) + "\",";
    j += "\"cat\":\"" + escapeJson(obs.category) + "\",";
    j += "\"freq\":" + String(obs.frequency, 3) + ",";
    j += "\"type\":\"" + String(patternTypeToString(obs.patternType)) + "\"";
    j += "}";
    return j;
}

String LearningManager::serializePattern(const LearnedPattern& p) const noexcept {
    String j; j.reserve(256);
    j += "{";
    j += "\"id\":\"" + escapeJson(p.id) + "\",";
    j += "\"name\":\"" + escapeJson(p.name) + "\",";
    j += "\"desc\":\"" + escapeJson(p.description) + "\",";
    j += "\"type\":\"" + String(patternTypeToString(p.type)) + "\",";
    j += "\"conf\":" + String(p.confidence, 3) + ",";
    j += "\"first\":" + String(p.firstObserved) + ",";
    j += "\"last\":" + String(p.lastObserved) + ",";
    j += "\"count\":" + String(p.occurrenceCount) + ",";
    j += "\"suggest\":\"" + escapeJson(p.suggestion) + "\",";
    j += "\"act\":" + String(p.actionable ? "true" : "false");
    j += "}";
    return j;
}

LearningObservation LearningManager::deserializeObservation(const String& json) const noexcept {
    LearningObservation obs;
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
    obs.id = eS("id");
    obs.timestamp = static_cast<unsigned long>(eI("ts", 0));
    obs.data = eS("data");
    obs.category = eS("cat");
    obs.frequency = eF("freq", 0.0f);
    String typeStr = eS("type");
    obs.patternType = stringToPatternType(typeStr);
    return obs;
}

LearnedPattern LearningManager::deserializePattern(const String& json) const noexcept {
    LearnedPattern p;
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
    p.id = eS("id");
    p.name = eS("name");
    p.description = eS("desc");
    String typeStr = eS("type");
    p.type = stringToPatternType(typeStr);
    p.confidence = eF("conf", 0.0f);
    p.firstObserved = static_cast<unsigned long>(eI("first", 0));
    p.lastObserved = static_cast<unsigned long>(eI("last", 0));
    p.occurrenceCount = static_cast<size_t>(eI("count", 0));
    p.suggestion = eS("suggest");
    p.actionable = eB("act", false);
    return p;
}
