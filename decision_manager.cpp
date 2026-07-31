#include "decision_manager.h"
#include "json_helpers.h"
#include <algorithm>

DecisionManager decisionManager;

DecisionManager::DecisionManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
    m_pendingOptions.reserve(DECISION_MAX_OPTIONS);
    m_history.reserve(kMaxHistory);
}

DecisionManager::~DecisionManager() noexcept {
    if (m_dirty) save();
}

bool DecisionManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(DECISIONS_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u records)", m_history.size());
    return true;
}

void DecisionManager::update() noexcept {
    if (!m_initialized) return;
    if (m_dirty) {
        if (save()) m_dirty = false;
    }
}

String DecisionManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

String DecisionManager::addOption(const String& name, const String& description,
                                   float priority, float risk, float deadlineUrgency) noexcept {
    if (!m_initialized || m_pendingOptions.size() >= DECISION_MAX_OPTIONS) return "";
    DecisionOption opt;
    opt.id = generateId();
    opt.name = name;
    opt.description = description;
    opt.priorityScore = constrain(priority, 0.0f, 1.0f);
    opt.riskScore = constrain(risk, 0.0f, 1.0f);
    opt.deadlineUrgency = constrain(deadlineUrgency, 0.0f, 1.0f);
    opt.timestamp = millis();
    m_pendingOptions.push_back(opt);
    return opt.id;
}

String DecisionManager::makeDecision(const String& question, const std::vector<String>& optionIds,
                                      const String& decisionType) noexcept {
    if (!m_initialized || optionIds.empty()) return "";
    
    DecisionRecord rec;
    rec.id = generateId();
    rec.question = question;
    rec.timestamp = millis();
    rec.decisionType = decisionType;
    
    float bestScore = -1.0f;
    String bestId;
    String reasoning;
    
    for (const auto& optId : optionIds) {
        // Find in pending options
        for (const auto& opt : m_pendingOptions) {
            if (opt.id != optId) continue;
            float confidence = computeConfidence(opt, decisionType);
            DecisionOption scored = opt;
            scored.confidenceScore = confidence;
            rec.options.push_back(scored);
            
            if (confidence > bestScore) {
                bestScore = confidence;
                bestId = optId;
            }
            break;
        }
    }
    
    rec.chosenOptionId = bestId;
    rec.overallConfidence = bestScore;
    rec.resolved = true;
    
    // Build reasoning
    reasoning = "Decision: '" + question + "' - ";
    if (!bestId.isEmpty()) {
        for (const auto& opt : rec.options) {
            if (opt.id == bestId) {
                reasoning += "Selected '" + opt.name + "' (confidence: " + String(bestScore, 2) + "). ";
                reasoning += "Priority: " + String(opt.priorityScore, 2) + ", Risk: " + String(opt.riskScore, 2);
                break;
            }
        }
    } else {
        reasoning += "No clear winner found.";
    }
    rec.reasoning = reasoning;
    
    m_history.push_back(rec);
    trimHistory();
    m_dirty = true;
    
    // Remove used options from pending
    for (const auto& oid : optionIds) {
        auto it = std::find_if(m_pendingOptions.begin(), m_pendingOptions.end(),
            [&](const DecisionOption& o) { return o.id == oid; });
        if (it != m_pendingOptions.end()) m_pendingOptions.erase(it);
    }
    
    return rec.id;
}

float DecisionManager::computeConfidence(const DecisionOption& option, const String& decisionType) const noexcept {
    float score = 0.5f;
    // Weighted scoring based on decision type
    if (decisionType == "time_management" || decisionType == "reminder_priority") {
        score = option.deadlineUrgency * 0.5f + option.priorityScore * 0.3f - option.riskScore * 0.2f;
    } else if (decisionType == "project" || decisionType == "task_selection") {
        score = option.priorityScore * 0.4f + (1.0f - option.riskScore) * 0.3f + option.deadlineUrgency * 0.3f;
    } else if (decisionType == "study") {
        score = option.priorityScore * 0.3f + (1.0f - option.riskScore) * 0.4f + option.deadlineUrgency * 0.3f;
    } else {
        score = option.priorityScore * 0.4f + (1.0f - option.riskScore) * 0.3f + option.deadlineUrgency * 0.3f;
    }
    return constrain(score, 0.0f, 1.0f);
}

String DecisionManager::explainDecision(const String& decisionId) const noexcept {
    for (const auto& rec : m_history) {
        if (rec.id != decisionId) continue;
        String detail; detail.reserve(512);
        detail += "Decision ID: " + rec.id + "\n";
        detail += "Question: " + rec.question + "\n";
        detail += "Type: " + rec.decisionType + "\n";
        detail += "Confidence: " + rec.confidence + " (" + String(rec.overallConfidence, 2) + ")\n";
        detail += "Reasoning: " + rec.reasoning + "\n";
        detail += "Alternatives: " + rec.alternatives + "\n";
        detail += "Chosen: " + rec.chosenOptionId;
        if (rec.explained) detail += "\nExplanation generated.";
        return detail;
    }
    return "Decision not found.";
}

float DecisionManager::assessRisk(const String& optionId) const noexcept {
    for (const auto& opt : m_pendingOptions) {
        if (opt.id == optionId) return opt.riskScore;
    }
    for (const auto& rec : m_history) {
        for (const auto& opt : rec.options) {
            if (opt.id == optionId) return opt.riskScore;
        }
    }
    return 1.0f;
}

std::vector<DecisionOption> DecisionManager::rankByPriority(const std::vector<String>& optionIds) const noexcept {
    std::vector<DecisionOption> ranked;
    for (const auto& oid : optionIds) {
        for (const auto& opt : m_pendingOptions) {
            if (opt.id == oid) { ranked.push_back(opt); break; }
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const DecisionOption& a, const DecisionOption& b) {
        return a.priorityScore > b.priorityScore;
    });
    return ranked;
}

const DecisionRecord* DecisionManager::getDecision(const String& id) const noexcept {
    for (const auto& rec : m_history) {
        if (rec.id == id) return &rec;
    }
    return nullptr;
}

std::vector<DecisionRecord> DecisionManager::getDecisionsByType(const String& decisionType) const noexcept {
    std::vector<DecisionRecord> results;
    for (const auto& rec : m_history) {
        if (rec.decisionType == decisionType) results.push_back(rec);
    }
    return results;
}

std::vector<DecisionRecord> DecisionManager::getDecisionsByConfidence(const String& confidenceLevel) const noexcept {
    std::vector<DecisionRecord> results;
    for (const auto& rec : m_history) {
        if (rec.confidence == confidenceLevel) results.push_back(rec);
    }
    return results;
}

std::vector<DecisionRecord> DecisionManager::getRecentDecisions(size_t count) const noexcept {
    std::vector<DecisionRecord> results;
    size_t start = (m_history.size() > count) ? m_history.size() - count : 0;
    for (size_t i = start; i < m_history.size(); ++i) {
        results.push_back(m_history[i]);
    }
    return results;
}

String DecisionManager::getDecisionsJson(const String& type) const noexcept {
    String json; json.reserve(4096);
    json += "{\"decisions\":[";
    bool first = true;
    for (const auto& rec : m_history) {
        if (!type.isEmpty() && rec.decisionType != type) continue;
        if (!first) json += ",";
        first = false;
        json += serializeRecord(rec);
    }
    json += "]}";
    return json;
}

bool DecisionManager::save() noexcept {
    String path = String(DECISIONS_PATH) + "/data.json";
    String json; json.reserve(8192);
    json += "{\"version\":1,\"decisions\":[";
    for (size_t i = 0; i < m_history.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeRecord(m_history[i]);
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    return (st == StorageStatus::SUCCESS);
}

bool DecisionManager::load() noexcept {
    String path = String(DECISIONS_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_history.clear();
    int pos = content.indexOf("\"decisions\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        DecisionRecord rec = deserializeRecord(obj);
        if (!rec.id.isEmpty()) m_history.push_back(rec);
        pos = braceEnd + 1;
    }
    return true;
}

bool DecisionManager::isInitialized() const noexcept { return m_initialized; }

void DecisionManager::trimHistory() noexcept {
    while (m_history.size() > kMaxHistory) {
        m_history.erase(m_history.begin());
    }
}

String DecisionManager::serializeOption(const DecisionOption& opt) const noexcept {
    String j; j.reserve(128);
    j += "{";
    j += "\"id\":\"" + escapeJson(opt.id) + "\",";
    j += "\"name\":\"" + escapeJson(opt.name) + "\",";
    j += "\"desc\":\"" + escapeJson(opt.description) + "\",";
    j += "\"prio\":" + String(opt.priorityScore, 3) + ",";
    j += "\"risk\":" + String(opt.riskScore, 3) + ",";
    j += "\"urg\":" + String(opt.deadlineUrgency, 3) + ",";
    j += "\"conf\":" + String(opt.confidenceScore, 3) + ",";
    j += "\"ts\":" + String(opt.timestamp);
    j += "}";
    return j;
}

String DecisionManager::serializeRecord(const DecisionRecord& rec) const noexcept {
    String j; j.reserve(1024);
    j += "{";
    j += "\"id\":\"" + escapeJson(rec.id) + "\",";
    j += "\"q\":\"" + escapeJson(rec.question) + "\",";
    j += "\"ts\":" + String(rec.timestamp) + ",";
    j += "\"type\":\"" + escapeJson(rec.decisionType) + "\",";
    j += "\"chosen\":\"" + escapeJson(rec.chosenOptionId) + "\",";
    j += "\"reason\":\"" + escapeJson(rec.reasoning) + "\",";
    j += "\"conf\":" + String(rec.overallConfidence, 3) + ",";
    j += "\"resolved\":" + String(rec.resolved ? "true" : "false") + ",";
    j += "\"alternatives\":\"" + escapeJson(rec.alternatives) + "\",";
    j += "\"confidence\":\"" + escapeJson(rec.confidence) + "\",";
    j += "\"explained\":" + String(rec.explained ? "true" : "false") + ",";
    j += "\"options\":[";
    for (size_t i = 0; i < rec.options.size(); ++i) {
        if (i > 0) j += ",";
        j += serializeOption(rec.options[i]);
    }
    j += "]}";
    return j;
}

DecisionOption DecisionManager::deserializeOption(const String& json, int& pos) const noexcept {
    DecisionOption opt;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s, pos);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    auto eF = [&](const char* key, float def) -> float {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s, pos);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && (json[end] == '-' || json[end] == '.' || (json[end] >= '0' && json[end] <= '9'))) end++;
        return (end == start) ? def : json.substring(start, end).toFloat();
    };
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s, pos);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    opt.id = eS("id");
    opt.name = eS("name");
    opt.description = eS("desc");
    opt.priorityScore = eF("prio", 0.5f);
    opt.riskScore = eF("risk", 0.5f);
    opt.deadlineUrgency = eF("urg", 0.0f);
    opt.confidenceScore = eF("conf", 0.0f);
    opt.timestamp = static_cast<unsigned long>(eI("ts", 0));
    return opt;
}

DecisionRecord DecisionManager::deserializeRecord(const String& json) const noexcept {
    DecisionRecord rec;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
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
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    auto eB = [&](const char* key, bool def) -> bool {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        return json.substring(start).startsWith("true");
    };
    rec.id = eS("id");
    rec.question = eS("q");
    rec.timestamp = static_cast<unsigned long>(eI("ts", 0));
    rec.decisionType = eS("type");
    rec.chosenOptionId = eS("chosen");
    rec.reasoning = eS("reason");
    rec.overallConfidence = eF("conf", 0.0f);
    rec.resolved = eB("resolved", false);
    rec.alternatives = eS("alternatives");
    if (rec.alternatives.isEmpty()) rec.alternatives = "[]";
    rec.confidence = eS("confidence");
    if (rec.confidence.isEmpty()) rec.confidence = "medium";
    rec.explained = eB("explained", false);
    
    // Parse options array
    int optStart = json.indexOf("\"options\":[");
    if (optStart >= 0) {
        int pos = json.indexOf('[', optStart) + 1;
        while (pos < (int)json.length()) {
            int braceStart = json.indexOf('{', pos);
            if (braceStart < 0) break;
            rec.options.push_back(deserializeOption(json, pos));
            pos = json.indexOf('}', pos) + 1;
            if (pos <= 0) break;
            // Skip comma if present
            if (pos < (int)json.length() && json[pos] == ',') pos++;
        }
    }
    
    return rec;
}
