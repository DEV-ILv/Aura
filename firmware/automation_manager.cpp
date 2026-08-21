#include "automation_manager.h"
#include <WiFi.h>
#include "json_helpers.h"
#include "led_ring.h"
#include "display_manager.h"
#include "sound_manager.h"
#include "memory_manager.h"
#include "reminder_manager.h"

AutomationManager automationManager;

AutomationManager::AutomationManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastEvaluation(0), m_lastIdCounter(0) {
}

AutomationManager::~AutomationManager() noexcept { if (m_dirty) save(); }

bool AutomationManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    loadNLPatterns();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u scripts, %u nl patterns)", m_scripts.size(), m_nlPatterns.size());
    return true;
}

void AutomationManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }

    // Evaluate enabled scripts every 10 seconds
    if (now - m_lastEvaluation > 10000UL) {
        m_lastEvaluation = now;
        evaluateAll();
    }
}

String AutomationManager::createScript(const String& name) noexcept {
    if (!m_initialized || m_scripts.size() >= kMaxScripts || name.isEmpty()) return "";
    AutoScript s;
    s.id = generateId(); s.name = name; s.createdAt = millis();
    m_scripts.push_back(s); m_dirty = true;
    Logger::info(kLogCategory, "Script '%s' created", name.c_str());
    return s.id;
}

bool AutomationManager::deleteScript(const String& id) noexcept {
    size_t idx = findScript(id);
    if (idx == SIZE_MAX) return false;
    m_scripts.erase(m_scripts.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true; return true;
}

bool AutomationManager::enableScript(const String& id) noexcept {
    size_t idx = findScript(id);
    if (idx == SIZE_MAX) return false;
    m_scripts[idx].enabled = true; m_dirty = true; return true;
}

bool AutomationManager::disableScript(const String& id) noexcept {
    size_t idx = findScript(id);
    if (idx == SIZE_MAX) return false;
    m_scripts[idx].enabled = false; m_dirty = true; return true;
}

bool AutomationManager::addCondition(const String& scriptId, const AutoCondition& cond) noexcept {
    size_t idx = findScript(scriptId);
    if (idx == SIZE_MAX || m_scripts[idx].conditions.size() >= AUTO_CONDITIONS_MAX) return false;
    m_scripts[idx].conditions.push_back(cond); m_dirty = true; return true;
}

bool AutomationManager::addAction(const String& scriptId, const AutoAction& action) noexcept {
    size_t idx = findScript(scriptId);
    if (idx == SIZE_MAX || m_scripts[idx].actions.size() >= AUTO_ACTIONS_MAX) return false;
    m_scripts[idx].actions.push_back(action); m_dirty = true; return true;
}

bool AutomationManager::addElseAction(const String& scriptId, const AutoAction& action) noexcept {
    size_t idx = findScript(scriptId);
    if (idx == SIZE_MAX || m_scripts[idx].elseActions.size() >= AUTO_ACTIONS_MAX) return false;
    m_scripts[idx].elseActions.push_back(action); m_dirty = true; return true;
}

bool AutomationManager::evaluateScript(const String& scriptId) noexcept {
    size_t idx = findScript(scriptId);
    if (idx == SIZE_MAX || !m_scripts[idx].enabled) return false;

    const auto& script = m_scripts[idx];
    bool allTrue = true;

    for (const auto& cond : script.conditions) {
        bool result = evaluateCondition(cond);
        if (cond.invert) result = !result;
        if (!result) { allTrue = false; break; }
    }

    if (allTrue) {
        for (const auto& action : script.actions) executeAction(action);
    } else {
        for (const auto& action : script.elseActions) executeAction(action);
    }

    return allTrue;
}

size_t AutomationManager::evaluateAll() noexcept {
    if (!m_initialized) return 0;
    size_t triggered = 0;
    for (const auto& s : m_scripts) {
        if (s.enabled && evaluateScript(s.id)) triggered++;
    }
    return triggered;
}

AutoScript AutomationManager::getScript(const String& id) const noexcept {
    size_t idx = findScript(id);
    return (idx != SIZE_MAX) ? m_scripts[idx] : AutoScript();
}

std::vector<AutoScript> AutomationManager::getEnabledScripts() const noexcept {
    std::vector<AutoScript> r;
    for (const auto& s : m_scripts) { if (s.enabled) r.push_back(s); }
    return r;
}

const std::vector<AutoScript>& AutomationManager::getAllScripts() const noexcept { return m_scripts; }
size_t AutomationManager::scriptCount() const noexcept { return m_scripts.size(); }
bool AutomationManager::isInitialized() const noexcept { return m_initialized; }

bool AutomationManager::evaluateCondition(const AutoCondition& cond) const noexcept {
    switch (cond.type) {
        case AutoConditionType::TIME: {
            // Simple time check - compare milliseconds of day
            unsigned long msPerDay = 86400000UL;
            unsigned long now = millis() % msPerDay;
            unsigned long target = cond.param1.toInt() * 3600000UL + cond.param2.toInt() * 60000UL;
            return now >= target;
        }
        case AutoConditionType::WIFI_STATE:
            return (WiFi.status() == WL_CONNECTED) == (cond.param1 == "connected");
        case AutoConditionType::MEMORY_COUNT:
            return memoryManager.memoryCount() >= static_cast<size_t>(cond.param1.toInt());
        case AutoConditionType::GOAL_STATUS:
            return true; // Simplified
        case AutoConditionType::HABIT_STATUS:
            return true; // Simplified
        case AutoConditionType::SYSTEM_STATE:
            return cond.param1 == "ready";
        default:
            return false;
    }
}

void AutomationManager::executeAction(const AutoAction& action) noexcept {
    Logger::info(kLogCategory, "Executing action: %s -> %s",
        actionTypeToString(action.type), action.target.c_str());

    switch (action.type) {
        case AutoActionType::LED_SET:
            Logger::warning(kLogCategory, "Action LED_SET not yet implemented");
            break;
        case AutoActionType::DISPLAY_SHOW:
            Logger::warning(kLogCategory, "Action DISPLAY_SHOW not yet implemented");
            break;
        case AutoActionType::SOUND_PLAY:
            Logger::warning(kLogCategory, "Action SOUND_PLAY not yet implemented");
            break;
        case AutoActionType::MEMORY_SAVE:
            memoryManager.remember(MemoryCategory::CUSTOM, action.target, action.value);
            break;
        case AutoActionType::SET_REMINDER:
            Logger::warning(kLogCategory, "Action SET_REMINDER not yet implemented");
            break;
        case AutoActionType::RUN_PLUGIN:
            Logger::warning(kLogCategory, "Action RUN_PLUGIN not yet implemented");
            break;
        case AutoActionType::CHANGE_SETTING:
            Logger::warning(kLogCategory, "Action CHANGE_SETTING not yet implemented");
            break;
    }
}

bool AutomationManager::save() noexcept {
    String json; json.reserve(4096);
    json += "{\"scripts\":[";
    for (size_t i = 0; i < m_scripts.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = m_scripts[i];
        json += "{\"id\":\"" + escapeJson(s.id) + "\",\"name\":\"" + escapeJson(s.name) + "\",";
        json += "\"enabled\":" + String(s.enabled ? "true" : "false") + "}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(AUTOMATIONS_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool AutomationManager::load() noexcept {
    if (!storageManager.fileExists(AUTOMATIONS_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(AUTOMATIONS_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_scripts.clear();
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
        AutoScript sc; sc.id = ext("id"); sc.name = ext("name");
        if (!sc.id.isEmpty()) { m_scripts.push_back(sc); }
        pos = e + 1;
    }
    return true;
}

void AutomationManager::saveNLPatterns() noexcept {
    String json; json.reserve(2048);
    json += "[";
    for (size_t i = 0; i < m_nlPatterns.size(); ++i) {
        if (i > 0) json += ",";
        const auto& p = m_nlPatterns[i];
        json += "{\"id\":\"" + escapeJson(p.id) + "\",";
        json += "\"pattern\":\"" + escapeJson(p.pattern) + "\",";
        json += "\"actionType\":\"" + escapeJson(p.actionType) + "\",";
        json += "\"actionParams\":\"" + escapeJson(p.actionParams) + "\",";
        json += "\"priority\":" + String(p.priority) + ",";
        json += "\"createdAt\":" + String(p.createdAt) + ",";
        json += "\"lastMatched\":" + String(p.lastMatched) + ",";
        json += "\"matchCount\":" + String(p.matchCount) + "}";
    }
    json += "]";
    StorageStatus st = storageManager.writeFile(kNLPatternsPath, json, StorageType::SPIFFS);
    if (st != StorageStatus::SUCCESS) {
        Logger::error(kLogCategory, "Failed to save NL patterns");
    }
}

void AutomationManager::loadNLPatterns() noexcept {
    if (!storageManager.fileExists(kNLPatternsPath, StorageType::SPIFFS)) return;
    String content;
    if (storageManager.readFile(kNLPatternsPath, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return;
    m_nlPatterns.clear();
    int pos = 0;
    while (true) {
        int s = content.indexOf('{', pos); if (s < 0) break;
        int e = content.indexOf('}', s); if (e < 0) break;
        String obj = content.substring(s, e + 1);
        auto extStr = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\""; int st = obj.indexOf(q);
            if (st < 0) { q = String("\"") + k + "\":"; st = obj.indexOf(q); if (st < 0) return ""; st += q.length(); int en = st; while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}' && obj[en] != ']') en++; return obj.substring(st, en); }
            st += q.length(); int en = obj.indexOf('"', st); return (en < 0) ? "" : obj.substring(st, en);
        };
        auto extInt = [&](const char* k) -> unsigned long {
            String q = String("\"") + k + "\":"; int st = obj.indexOf(q);
            if (st < 0) return 0; st += q.length();
            int en = st; while (en < (int)obj.length() && obj[en] >= '0' && obj[en] <= '9') en++;
            return (en > st) ? strtoul(obj.substring(st, en).c_str(), nullptr, 10) : 0;
        };
        NLPattern p;
        p.id = extStr("id");
        if (p.id.isEmpty()) { pos = e + 1; continue; }
        p.pattern = extStr("pattern");
        p.actionType = extStr("actionType");
        p.actionParams = extStr("actionParams");
        p.priority = (uint8_t)extInt("priority");
        p.createdAt = extInt("createdAt");
        p.lastMatched = extInt("lastMatched");
        p.matchCount = (uint32_t)extInt("matchCount");
        m_nlPatterns.push_back(p);
        pos = e + 1;
    }
}

bool AutomationManager::addNLPattern(const NLPattern& pattern) noexcept {
    if (m_nlPatterns.size() >= kMaxNLPatterns) {
        Logger::warning(kLogCategory, "Max NL patterns reached");
        return false;
    }
    NLPattern p = pattern;
    if (p.id.isEmpty()) {
        p.id = generateId();
    }
    p.createdAt = millis();
    m_nlPatterns.push_back(p);
    m_dirty = true;
    Logger::info(kLogCategory, "NL pattern '%s' added", p.pattern.c_str());
    return true;
}

bool AutomationManager::removeNLPattern(const String& patternId) noexcept {
    size_t idx = findNLPattern(patternId);
    if (idx == SIZE_MAX) return false;
    m_nlPatterns.erase(m_nlPatterns.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;
    return true;
}

NLPattern AutomationManager::matchNL(const String& text) const noexcept {
    NLPattern best;
    int8_t bestPriority = -1;
    for (const auto& p : m_nlPatterns) {
        if (wildcardMatch(text, p.pattern)) {
            if (static_cast<int8_t>(p.priority) > bestPriority) {
                best = p;
                bestPriority = static_cast<int8_t>(p.priority);
            }
        }
    }
    return best;
}

std::vector<NLPattern> AutomationManager::getAllNLPatterns() const noexcept {
    return m_nlPatterns;
}

String AutomationManager::parseNLToAction(const String& text) const noexcept {
    NLPattern p = matchNL(text);
    if (p.id.isEmpty()) return "";
    String json;
    json.reserve(256);
    json += "{\"type\":\"" + escapeJson(p.actionType) + "\",\"params\":" + p.actionParams + "}";
    return json;
}

bool AutomationManager::wildcardMatch(const String& text, const String& pattern) const noexcept {
    size_t textPos = 0;
    int prevSegEnd = 0;
    for (int i = 0; i <= (int)pattern.length(); ++i) {
        if (i == (int)pattern.length() || pattern[i] == '*') {
            String seg = pattern.substring(prevSegEnd, i);
            if (seg.length() > 0) {
                size_t found = text.indexOf(seg, textPos);
                if (found == SIZE_MAX) return false;
                textPos = found + seg.length();
            }
            prevSegEnd = i + 1;
        }
    }
    return true;
}

size_t AutomationManager::findNLPattern(const String& id) const noexcept {
    for (size_t i = 0; i < m_nlPatterns.size(); ++i) {
        if (m_nlPatterns[i].id == id) return i;
    }
    return SIZE_MAX;
}

String AutomationManager::generateId() noexcept {
    return ::generateId();
}

size_t AutomationManager::findScript(const String& id) const noexcept {
    for (size_t i = 0; i < m_scripts.size(); ++i) { if (m_scripts[i].id == id) return i; }
    return SIZE_MAX;
}

const char* AutomationManager::conditionTypeToString(AutoConditionType t) const noexcept {
    switch (t) {
        case AutoConditionType::TIME: return "TIME";
        case AutoConditionType::WIFI_STATE: return "WIFI_STATE";
        case AutoConditionType::MEMORY_COUNT: return "MEMORY_COUNT";
        case AutoConditionType::GOAL_STATUS: return "GOAL_STATUS";
        case AutoConditionType::HABIT_STATUS: return "HABIT_STATUS";
        case AutoConditionType::SYSTEM_STATE: return "SYSTEM_STATE";
        default: return "UNKNOWN";
    }
}

AutoConditionType AutomationManager::stringToConditionType(const String& s) const noexcept {
    if (s == "TIME") return AutoConditionType::TIME;
    if (s == "WIFI_STATE") return AutoConditionType::WIFI_STATE;
    if (s == "MEMORY_COUNT") return AutoConditionType::MEMORY_COUNT;
    if (s == "GOAL_STATUS") return AutoConditionType::GOAL_STATUS;
    if (s == "HABIT_STATUS") return AutoConditionType::HABIT_STATUS;
    if (s == "SYSTEM_STATE") return AutoConditionType::SYSTEM_STATE;
    return AutoConditionType::TIME;
}

const char* AutomationManager::actionTypeToString(AutoActionType t) const noexcept {
    switch (t) {
        case AutoActionType::LED_SET: return "LED_SET";
        case AutoActionType::DISPLAY_SHOW: return "DISPLAY_SHOW";
        case AutoActionType::SOUND_PLAY: return "SOUND_PLAY";
        case AutoActionType::MEMORY_SAVE: return "MEMORY_SAVE";
        case AutoActionType::SET_REMINDER: return "SET_REMINDER";
        case AutoActionType::RUN_PLUGIN: return "RUN_PLUGIN";
        case AutoActionType::CHANGE_SETTING: return "CHANGE_SETTING";
        default: return "UNKNOWN";
    }
}

AutoActionType AutomationManager::stringToActionType(const String& s) const noexcept {
    if (s == "LED_SET") return AutoActionType::LED_SET;
    if (s == "DISPLAY_SHOW") return AutoActionType::DISPLAY_SHOW;
    if (s == "SOUND_PLAY") return AutoActionType::SOUND_PLAY;
    if (s == "MEMORY_SAVE") return AutoActionType::MEMORY_SAVE;
    if (s == "SET_REMINDER") return AutoActionType::SET_REMINDER;
    if (s == "RUN_PLUGIN") return AutoActionType::RUN_PLUGIN;
    if (s == "CHANGE_SETTING") return AutoActionType::CHANGE_SETTING;
    return AutoActionType::LED_SET;
}
