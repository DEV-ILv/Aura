#include "prediction_manager.h"
#include <WiFi.h>
#include "json_helpers.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "reminder_manager.h"
#include "storage_manager.h"
#include <algorithm>
#include <math.h>

PredictionManager predictionManager;

PredictionManager::PredictionManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0), m_lastRunTime(0) {
}

PredictionManager::~PredictionManager() noexcept {
    if (m_dirty) save();
}

bool PredictionManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(PREDICTIONS_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u predictions)", m_predictions.size());
    return true;
}

void PredictionManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastRunTime >= kIntervalMs) {
        m_lastRunTime = now;
        runAllPredictions();
    }
    if (m_dirty && save()) m_dirty = false;
}

String PredictionManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

void PredictionManager::runAllPredictions() noexcept {
    // Goal completion predictions
    if (goalManager.isInitialized()) {
        auto goals = goalManager.getAllGoals();
        for (const auto& g : goals) {
            if (g.completed) continue;
            float prob = predictGoalCompletion(g.id, g.progress, g.deadline);
            if (prob > 0.3f) {
                addPrediction(PredictionType::GOAL_COMPLETION, g.id, g.title, prob, 0.4f, g.deadline,
                    "Goal '" + g.title + "' has " + String(prob * 100, 0) + "% completion probability. Progress: " + String(g.progress) + "%");
            }
        }
    }
    
    // Habit predictions
    if (habitManager.isInitialized()) {
        auto habits = habitManager.getAllHabits();
        for (const auto& h : habits) {
            float prob = predictMissedHabit(h.id, static_cast<float>(h.streak), h.successRate);
            if (prob > 0.3f) {
                addPrediction(PredictionType::MISSED_HABIT, h.id, h.name, prob, 0.5f, 0,
                    "Habit '" + h.name + "' may be missed (confidence: " + String(prob * 100, 0) + "%)");
            }
        }
    }
    
    // Storage exhaustion prediction
    size_t total = storageManager.getTotalSpace();
    size_t freeB = storageManager.getFreeSpace();
    float dailyUsage = 512.0f; // Assume ~512 bytes/day average (simplified)
    float storageProb = predictStorageExhaustion(total, freeB, dailyUsage);
    if (storageProb > 0.3f) {
        addPrediction(PredictionType::STORAGE_EXHAUSTION, "", "Storage", storageProb, 0.6f,
            static_cast<unsigned long>(millis() + static_cast<unsigned long>(freeB / dailyUsage) * 86400000UL),
            "Storage may run out. Free: " + String(freeB / 1024) + "KB / " + String(total / 1024) + "KB");
    }
    
    // WiFi failure prediction
    float wifiProb = predictWiFiFailure(WiFi.RSSI(), 0);
    if (wifiProb > 0.3f) {
        addPrediction(PredictionType::WIFI_FAILURE, "", "WiFi", wifiProb, 0.3f, 0, "WiFi signal strength is weak (" + String(WiFi.RSSI()) + " dBm)");
    }
}

float PredictionManager::predictGoalCompletion(const String& goalId, float currentProgress, unsigned long deadline) noexcept {
    if (deadline == 0) return 0.5f;
    unsigned long now = millis();
    if (now >= deadline) return currentProgress / 100.0f;
    
    float timeRatio = static_cast<float>(deadline - now) / 86400000.0f; // days remaining
    if (timeRatio <= 0) return currentProgress / 100.0f;
    
    float progressRate = currentProgress / (std::max)(1.0f, timeRatio);
    float daysNeeded = (100.0f - currentProgress) / (std::max)(progressRate, 0.1f);
    
    if (daysNeeded <= timeRatio) return 0.8f + (currentProgress / 500.0f);
    if (daysNeeded <= timeRatio * 1.5f) return 0.5f;
    return 0.2f;
}

float PredictionManager::predictMissedHabit(const String& habitId, float streak, float successRate) noexcept {
    if (successRate <= 0) return 0.8f;
    float prob = 1.0f - (successRate / 100.0f);
    // Longer streaks reduce miss probability
    prob -= (std::min)(streak / 100.0f, 0.3f);
    return constrain(prob, 0.0f, 1.0f);
}

float PredictionManager::predictLateReminder(const String& reminderId, unsigned long triggerTime) noexcept {
    unsigned long now = millis();
    if (triggerTime <= now) return 1.0f; // Already late
    float timeUntilDue = static_cast<float>(triggerTime - now) / 3600000.0f; // hours
    if (timeUntilDue <= 1.0f) return 0.8f;
    if (timeUntilDue <= 4.0f) return 0.4f;
    return 0.1f;
}

float PredictionManager::predictStorageExhaustion(size_t totalBytes, size_t freeBytes, float dailyUsageBytes) noexcept {
    if (freeBytes == 0) return 1.0f;
    if (dailyUsageBytes <= 0) return 0.1f;
    float daysUntilFull = static_cast<float>(freeBytes) / dailyUsageBytes;
    if (daysUntilFull <= 7) return 0.9f;
    if (daysUntilFull <= 30) return 0.6f;
    if (daysUntilFull <= 90) return 0.3f;
    return 0.1f;
}

float PredictionManager::predictWiFiFailure(int rssi, int disconnects) noexcept {
    float prob = 0.0f;
    if (rssi < -80) prob += 0.5f;
    else if (rssi < -70) prob += 0.3f;
    else if (rssi < -60) prob += 0.1f;
    prob += (std::min)(static_cast<float>(disconnects) * 0.1f, 0.3f);
    return constrain(prob, 0.0f, 1.0f);
}

float PredictionManager::predictProductivityTrend(const std::vector<float>& history) noexcept {
    if (history.size() < 2) return 0.5f;
    float sum = 0;
    for (size_t i = 1; i < history.size(); ++i) {
        sum += history[i] - history[i-1];
    }
    float avgDelta = sum / (history.size() - 1);
    return constrain(0.5f + avgDelta, 0.0f, 1.0f);
}

String PredictionManager::addPrediction(PredictionType type, const String& targetId,
                                          const String& targetName, float probability,
                                          float confidence, unsigned long predictedTime,
                                          const String& reasoning) noexcept {
    if (!m_initialized) return "";
    
    Prediction p;
    p.id = generateId();
    p.timestamp = millis();
    p.type = type;
    p.targetId = targetId;
    p.targetName = targetName;
    p.probability = constrain(probability, 0.0f, 1.0f);
    p.confidence = constrain(confidence, 0.0f, 1.0f);
    p.predictedTime = predictedTime;
    p.reasoning = reasoning;
    m_predictions.push_back(p);
    trimHistory();
    m_dirty = true;
    return p.id;
}

std::vector<Prediction> PredictionManager::getActivePredictions(float minProbability) const noexcept {
    std::vector<Prediction> active;
    for (const auto& p : m_predictions) {
        if (p.probability >= minProbability && !p.verified) {
            active.push_back(p);
        }
    }
    return active;
}

std::vector<Prediction> PredictionManager::getByType(PredictionType type) const noexcept {
    std::vector<Prediction> results;
    for (const auto& p : m_predictions) {
        if (p.type == type) results.push_back(p);
    }
    return results;
}

const Prediction* PredictionManager::getPrediction(const String& id) const noexcept {
    for (const auto& p : m_predictions) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

String PredictionManager::getPredictionsJson(float minProbability) const noexcept {
    String json; json.reserve(4096);
    json += "{\"predictions\":[";
    bool first = true;
    for (const auto& p : m_predictions) {
        if (p.probability < minProbability) continue;
        if (!first) json += ",";
        first = false;
        json += serializePrediction(p);
    }
    json += "]}";
    return json;
}

bool PredictionManager::save() noexcept {
    String path = String(PREDICTIONS_PATH) + "/data.json";
    String j; j.reserve(8192);
    j += "{\"version\":1,\"predictions\":[";
    for (size_t i = 0; i < m_predictions.size(); ++i) {
        if (i > 0) j += ",";
        j += serializePrediction(m_predictions[i]);
    }
    j += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), j, StorageType::SPIFFS);
    return (st == StorageStatus::SUCCESS);
}

bool PredictionManager::load() noexcept {
    String path = String(PREDICTIONS_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_predictions.clear();
    int pos = content.indexOf("\"predictions\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        Prediction p = deserializePrediction(obj);
        if (!p.id.isEmpty()) m_predictions.push_back(p);
        pos = braceEnd + 1;
    }
    return true;
}

bool PredictionManager::isInitialized() const noexcept { return m_initialized; }

void PredictionManager::trimHistory() noexcept {
    while (m_predictions.size() > kMaxHistory) {
        m_predictions.erase(m_predictions.begin());
    }
}

const char* PredictionManager::typeToString(PredictionType t) const noexcept {
    switch (t) {
        case PredictionType::GOAL_COMPLETION: return "goal_completion";
        case PredictionType::MISSED_HABIT: return "missed_habit";
        case PredictionType::LATE_REMINDER: return "late_reminder";
        case PredictionType::STORAGE_EXHAUSTION: return "storage_exhaustion";
        case PredictionType::WIFI_FAILURE: return "wifi_failure";
        case PredictionType::LOW_BATTERY: return "low_battery";
        case PredictionType::LONG_RESPONSE_TIME: return "long_response";
        case PredictionType::PRODUCTIVITY_TREND: return "productivity_trend";
        default: return "custom";
    }
}

PredictionType PredictionManager::stringToType(const String& s) const noexcept {
    if (s == "goal_completion") return PredictionType::GOAL_COMPLETION;
    if (s == "missed_habit") return PredictionType::MISSED_HABIT;
    if (s == "late_reminder") return PredictionType::LATE_REMINDER;
    if (s == "storage_exhaustion") return PredictionType::STORAGE_EXHAUSTION;
    if (s == "wifi_failure") return PredictionType::WIFI_FAILURE;
    if (s == "low_battery") return PredictionType::LOW_BATTERY;
    if (s == "long_response") return PredictionType::LONG_RESPONSE_TIME;
    if (s == "productivity_trend") return PredictionType::PRODUCTIVITY_TREND;
    return PredictionType::CUSTOM;
}

String PredictionManager::serializePrediction(const Prediction& p) const noexcept {
    String j; j.reserve(256);
    j += "{";
    j += "\"id\":\"" + escapeJson(p.id) + "\",";
    j += "\"ts\":" + String(p.timestamp) + ",";
    j += "\"type\":\"" + String(typeToString(p.type)) + "\",";
    j += "\"tid\":\"" + escapeJson(p.targetId) + "\",";
    j += "\"tname\":\"" + escapeJson(p.targetName) + "\",";
    j += "\"prob\":" + String(p.probability, 3) + ",";
    j += "\"conf\":" + String(p.confidence, 3) + ",";
    j += "\"ptime\":" + String(p.predictedTime) + ",";
    j += "\"reason\":\"" + escapeJson(p.reasoning) + "\",";
    j += "\"ver\":" + String(p.verified ? "true" : "false");
    j += "}";
    return j;
}

Prediction PredictionManager::deserializePrediction(const String& json) const noexcept {
    Prediction p;
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
    p.timestamp = static_cast<unsigned long>(eI("ts", 0));
    String typeStr = eS("type");
    p.type = stringToType(typeStr);
    p.targetId = eS("tid");
    p.targetName = eS("tname");
    p.probability = eF("prob", 0.0f);
    p.confidence = eF("conf", 0.5f);
    p.predictedTime = static_cast<unsigned long>(eI("ptime", 0));
    p.reasoning = eS("reason");
    p.verified = eB("ver", false);
    return p;
}
